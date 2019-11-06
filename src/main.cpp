#include <cstdlib>
#include <cstdatomic>
#include <iostream>
#include <list>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include "Hash.hpp"
#include "StringManip.hpp"

namespace BAIp = boost::asio::ip;

namespace
{
  const bool DEBUG_OUTPUT = false; // true;
}

/**
 * class Connection
 */
class Connection:
  public std::enable_shared_from_this<Connection>,
  private boost::noncopyable
{
public:
  enum {
    READ_BUF_SIZE = 4*1024,
    // MAX_WRITE_BUF_SIZE: limit of memory buffer for write to socket
    // if it reached stop reading and wait when client will be agree to get some
    // portion of responses (actual in pipelining mode)
    MAX_WRITE_BUF_SIZE = 1000000
  };

  Connection(boost::asio::io_service& io_service);

  BAIp::tcp::socket&
  socket();

  void
  activate();

  void
  deactivate();

  void
  handle_read(
    const boost::system::error_code& error,
    size_t bytes_transferred);

  void
  handle_write(const boost::system::error_code& error);

private:
  void
  order_read_();

  bool
  order_write_();

  bool
  process_read_data_(size_t bytes_transferred);

  bool
  process_message_(
    const char* buf_part_begin,
    const char* buf_part_end);

private:
  BAIp::tcp::socket socket_;
  bool active_;

  // reading
  char data_[READ_BUF_SIZE];
  std::atomic<int> read_ordered_;
  // collect hash for currently received buffer
  // (hold not full chunk for hash evaluate step - see Aggregator in Hash.hpp)
  // used seed = 0
  Gears::Murmur32v3Hasher hasher_;

  // writing
  std::mutex send_buf_lock_;
  std::string send_buf_;

  std::atomic<int> write_ordered_;
  std::string ordered_send_buf_;
};

typedef std::shared_ptr<Connection> Connection_var;

/**
 * class Server
 */
class Server: private boost::noncopyable
{
public:
  Server(boost::asio::io_service& io_service, short port)
    throw(std::exception);

  void
  handle_accept(
    const Connection_var& accepted_connection,
    const boost::system::error_code& error);

private:
  void
  create_accept_stub_();

private:
  boost::asio::io_service& io_service_;
  BAIp::tcp::acceptor acceptor_;
};

typedef std::shared_ptr<Server> Server_var;

//
// Connection implementation
//
Connection::Connection(boost::asio::io_service& io_service)
  : socket_(io_service),
    active_(false),
    read_ordered_(0),
    write_ordered_(0)
{}

BAIp::tcp::socket&
Connection::socket()
{
  return socket_;
}

void
Connection::activate()
{
  active_ = true;
  order_read_();
}

void
Connection::deactivate()
{
  if(active_)
  {
    active_ = false;
    socket_.close();
  }
}

void
Connection::handle_read(
  const boost::system::error_code& error,
  size_t bytes_transferred)
{
  if(DEBUG_OUTPUT)
  {
    std::ostringstream ostr;
    ostr << "> Connection::handle_read()\n";
    std::cout << ostr.str();
    std::cout.flush();
  }

  if(!error)
  {
    // process got buffer
    // next read can be canceled if output buffer(send_buf_) reached size limit
    bool order_read = process_read_data_(bytes_transferred);

    --read_ordered_;

    order_read |= order_write_();

    if(order_read)
    {
      order_read_();
    }
  }
  else
  {
    // destroy & close on ref count == 0
  }

  if(DEBUG_OUTPUT)
  {
    std::ostringstream ostr;
    ostr << "< Connection::handle_read()\n";
    std::cout << ostr.str();
    std::cout.flush();
  }
}

void
Connection::handle_write(const boost::system::error_code& error)
{
  if(DEBUG_OUTPUT)
  {
    std::ostringstream ostr;
    ostr << "> Connection::handle_write()\n";
    std::cout << ostr.str();
    std::cout.flush();
  }

  if(!error)
  {
    // writing done
    --write_ordered_;

    if(order_write_())
    {
      // order read if read loop stopped and wait when will be
      // decreased output buffer (see MAX_WRITE_BUF_SIZE definition)
      order_read_();
    }
  }
  else
  {
    // destroy & close on ref count == 0
  }

  if(DEBUG_OUTPUT)
  {
    std::ostringstream ostr;
    ostr << "< Connection::handle_write()\n";
    std::cout << ostr.str();
    std::cout.flush();
  }
}

void
Connection::order_read_()
{
  if(++read_ordered_ == 1)
  {
    if(DEBUG_OUTPUT)
    {
      std::ostringstream ostr;
      ostr << "Connection::order_read_() : async_read_some\n";
      std::cout << ostr.str();
      std::cout.flush();
    }

    socket_.async_read_some(
      boost::asio::buffer(data_, READ_BUF_SIZE),
      boost::bind(
        &Connection::handle_read,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  }
  else
  {
    --read_ordered_;
  }
}

bool
Connection::order_write_()
{
  bool order_read = false;

  if(++write_ordered_ == 1)
  {
    // pass buffer that point to ordered_send_buf_
    {
      std::unique_lock<std::mutex> lock(send_buf_lock_);
      ordered_send_buf_.clear();
      order_read = (send_buf_.size() > MAX_WRITE_BUF_SIZE);
      send_buf_.swap(ordered_send_buf_);
    }

    if(!ordered_send_buf_.empty())
    {
      if(DEBUG_OUTPUT)
      {
        std::ostringstream ostr;
        ostr << "Connection::order_write_() : async_write\n";
        std::cout << ostr.str();
        std::cout.flush();
      }

      boost::asio::async_write(
        socket_,
        boost::asio::buffer(ordered_send_buf_.data(), ordered_send_buf_.size()),
        boost::bind(
          &Connection::handle_write,
          shared_from_this(),
          boost::asio::placeholders::error));
    }
    else
    {
      --write_ordered_;
    }
  }
  else
  {
    --write_ordered_;
  }

  return order_read;
}

bool
Connection::process_read_data_(size_t bytes_transferred)
{
  // separate buffer by '\n' (first part process with accumulated hasher state)
  bool order_next_read = true;
  const char* data_start = data_;
  const char* data_end = data_ + bytes_transferred;

  while(true)
  {
    const char* message_end = std::find(data_start, data_end, '\n');

    if(message_end != data_end)
    {
      order_next_read = process_message_(data_start, message_end);
      data_start = message_end + 1;
    }
    else
    {
      break;
    }
  }

  if(data_start != data_end)
  {
    // update hasher for partly got request (last part of buffer)
    hasher_.add(data_start, data_end - data_start);
  }

  return order_next_read;
}

bool
Connection::process_message_(
  const char* buf_part_begin,
  const char* buf_part_end)
{
  assert(buf_part_begin <= buf_part_end);

  bool order_next_read = true;

  // append response to send_buf_
  hasher_.add(buf_part_begin, buf_part_end - buf_part_begin);
  uint32_t response_hash = hasher_.finalize();

  // convert value to hex representation in upcase latters alphabeth
  // if required lowcase letters (a-f) switch to hex_low_encode
  const std::string response = Gears::StringManip::hex_encode(
    reinterpret_cast<unsigned char*>(&response_hash),
    sizeof(response_hash),
    false // if you need to skip leading zeroes in response set to true
    );

  {
    std::unique_lock<std::mutex> lock(send_buf_lock_);
    send_buf_ += response;
    send_buf_ += '\n';
    if(send_buf_.size() > MAX_WRITE_BUF_SIZE)
    {
      order_next_read = false;
    }
  }

  // reset hasher
  hasher_ = Gears::Murmur32v3Hasher();

  return order_next_read;
}

// Server implementation
Server::Server(boost::asio::io_service& io_service, short port)
  throw(std::exception)
  : io_service_(io_service),
    // create acceptor in listen mode with reuse address option
    acceptor_(
      io_service_,
      BAIp::tcp::endpoint(BAIp::tcp::v4(), port))
{
  create_accept_stub_();
}

void
Server::handle_accept(
  const Connection_var& accepted_connection,
  const boost::system::error_code& error)
{
  if(DEBUG_OUTPUT)
  {
    std::ostringstream ostr;
    ostr << "Server::handle_accept()\n";
    std::cout << ostr.str();
    std::cout.flush();
  }

  if(!error)
  {
    // activate stub as normal connection
    accepted_connection->activate();

    create_accept_stub_();
  }
  else
  {
    // descriptors limit reached ? don't stop
    std::cerr << "Can't accept connection: " << error << std::endl;
  }
}

void
Server::create_accept_stub_()
{
  // create stub for new connection
  Connection_var new_connection(new Connection(io_service_));
  acceptor_.async_accept(
    new_connection->socket(),
    boost::bind(
      &Server::handle_accept,
      this,
      new_connection,
      boost::asio::placeholders::error));
}

// IOServicePool implementation

// main
int
main(int argc, char* argv[])
{
  try
  {
    // parse options
    short listen_port = 10000;
    unsigned int threads = std::max(std::min(std::thread::hardware_concurrency(), 12u), 1u);

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
      ("help,h", "print usage message")
      ("port,p", boost::program_options::value<short>(&listen_port),
        "server listen port (10000 by default)")
      ("threads,t", boost::program_options::value<unsigned int>(&threads),
        "number of threads for process requests")
      ;

    boost::program_options::variables_map vm;
    boost::program_options::store(
      boost::program_options::command_line_parser(argc, argv).options(desc).run(),
      vm);
    boost::program_options::notify(vm); // store values into variables

    // start asio
    boost::asio::io_service io_service;
    boost::asio::signal_set signals{io_service, SIGUSR1};
    signals.async_wait(std::bind(&boost::asio::io_service::stop, &io_service));

    std::cout << "Start on port " << listen_port << " with " << threads << " threads" << std::endl;

    Server s(io_service, listen_port);
    std::list<std::thread> io_threads;
    for(unsigned int thread_i = 0; thread_i < threads; ++thread_i)
    {
      io_threads.emplace_back(
        std::thread(boost::bind(&boost::asio::io_service::run, &io_service)));
    }

    for(auto thread_it = io_threads.begin(); thread_it != io_threads.end(); ++thread_it)
    {
      thread_it->join();
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
