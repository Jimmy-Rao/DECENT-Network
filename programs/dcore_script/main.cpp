
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/server.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/http_api.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/interprocess/signals.hpp>


#include <graphene/app/api.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/wallet/wallet.hpp>
#include <decent/package/package.hpp>
#include <graphene/utilities/dirhelper.hpp>

#include <boost/program_options.hpp>


#include "cmd_interpret.h"


using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace graphene::wallet;
namespace bpo = boost::program_options;


///////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
   fc::path decent_home;
   try {
      decent_home = decent_path_finder::instance().get_decent_home();
   } catch (const std::exception& ex) {
      std::cout << "Failed to initialize home directory." << std::endl;
      std::cout << "Error: " << ex.what() << std::endl;
      return 1;
   } catch (const fc::exception& ex) {
      std::cout << "Failed to initialize home directory." << std::endl;
      std::cout << "Error: " << ex.what() << std::endl;
      return 1;
   }

   try {

      boost::program_options::options_description opts;
      opts.add_options()
            ("help,h", "Print this help message and exit.")
            ("server-rpc-endpoint,s", bpo::value<std::string>()->implicit_value("ws://127.0.0.1:8090"), "Server websocket RPC endpoint")
            ("server-rpc-user,u", bpo::value<std::string>(), "Server Username")
            ("server-rpc-password,p", bpo::value<std::string>(), "Server Password")
            ("rpc-endpoint,r", bpo::value<std::string>()->implicit_value("127.0.0.1:8091"), "Endpoint for wallet websocket RPC to listen on")
            ("rpc-tls-endpoint,t", bpo::value<std::string>()->implicit_value("127.0.0.1:8092"), "Endpoint for wallet websocket TLS RPC to listen on")
            ("rpc-tls-certificate,c", bpo::value<std::string>()->implicit_value("server.pem"), "PEM certificate for wallet websocket TLS RPC")
            ("rpc-http-endpoint,H", bpo::value<std::string>()->implicit_value("127.0.0.1:8093"), "Endpoint for wallet HTTP RPC to listen on")
            ("daemon,d", "Run the wallet in daemon mode" )
            ("wallet-file,w", bpo::value<std::string>()->implicit_value("wallet.json"), "wallet to load")
            ("chain-id", bpo::value<std::string>(), "chain ID to connect to");

      bpo::variables_map options;

      bpo::store( bpo::parse_command_line(argc, argv, opts), options );

      if( options.count("help") )
      {
         std::cout << opts << "\n";
         return 0;
      }


      fc::path data_dir;
      fc::logging_config cfg;
      const fc::path log_dir = decent_path_finder::instance().get_decent_logs();

      fc::file_appender::config ac_default;

      fc::file_appender::config ac_rpc;
      ac_rpc.filename             = log_dir / "rpc" / "rpc.log";
      ac_rpc.flush                = true;
      ac_rpc.rotate               = true;
      ac_rpc.rotation_interval    = fc::hours( 1 );
      ac_rpc.rotation_limit       = fc::days( 1 );

      fc::file_appender::config ac_transfer;
      ac_transfer.format               = "${timestamp} ${thread_name} ${context} ${level}]  ${message}";
      ac_transfer.filename             = log_dir / "transfer.log";
      ac_transfer.flush                = true;
      ac_transfer.rotate               = true;
      ac_transfer.rotation_interval    = fc::hours( 1 );
      ac_transfer.rotation_limit       = fc::days( 1 );

//    cfg.appenders.push_back(fc::appender_config( "default", "console", fc::variant(ac_default)));
//    cfg.appenders.push_back(fc::appender_config( "rpc", "file", fc::variant(ac_rpc)));
      cfg.appenders.push_back(fc::appender_config( "transfer", "file", fc::variant(ac_transfer)));

      fc::logger_config lc_default("default");
      lc_default.level          = fc::log_level::info;
      lc_default.appenders      = {"default"};

      fc::logger_config lc_rpc("rpc");
      lc_rpc.level              = fc::log_level::debug;
      lc_rpc.appenders          = {"rpc"};

      fc::logger_config lc_transfer("transfer");
      lc_transfer.level         = fc::log_level::debug;
      lc_transfer.appenders     = {"transfer"};

//    cfg.loggers.push_back(lc_default);
//    cfg.loggers.push_back(lc_rpc);
      cfg.loggers.push_back(lc_transfer);

      std::clog << "Logging RPC to file: " << ac_rpc.filename.preferred_string() << std::endl;
      std::clog << "Logging transfers to file: " << ac_transfer.filename.preferred_string() << std::endl;

      fc::configure_logging( cfg );

      fc::ecc::private_key committee_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("null_key")));

      idump( (key_to_wif( committee_private_key ) ) );

      //
      // TODO:  We read wallet_data twice, once in main() to grab the
      //    socket info, again in wallet_api when we do
      //    load_wallet_file().  Seems like this could be better
      //    designed.
      //
      wallet_data wdata;

      fc::path wallet_file( options.count("wallet-file") ? options.at("wallet-file").as<std::string>() : decent_path_finder::instance().get_decent_home() / "wallet.json");

      if( fc::exists( wallet_file ) )
      {
         wdata = fc::json::from_file( wallet_file ).as<wallet_data>();
         if( options.count("chain-id") )
         {
            // the --chain-id on the CLI must match the chain ID embedded in the wallet file
            if( chain_id_type(options.at("chain-id").as<std::string>()) != wdata.chain_id )
            {
               std::cout << "Chain ID in wallet file does not match specified chain ID\n";
               return 1;
            }
         }
      }
      else
      {
         if( options.count("chain-id") )
         {
            wdata.chain_id = chain_id_type(options.at("chain-id").as<std::string>());
            std::cout << "Starting a new wallet with chain ID " << wdata.chain_id.str() << " (from CLI)\n";
         }
         else
         {
            wdata.chain_id = chain_id_type ("0000000000000000000000000000000000000000000000000000000000000000"); //graphene::egenesis::get_egenesis_chain_id();
            std::cout << "Starting a new wallet with chain ID " << wdata.chain_id.str() << " (empty one)\n";
         }
      }

      // but allow CLI to override
      if( options.count("server-rpc-endpoint") )
         wdata.ws_server = options.at("server-rpc-endpoint").as<std::string>();
      if( options.count("server-rpc-user") )
         wdata.ws_user = options.at("server-rpc-user").as<std::string>();
      if( options.count("server-rpc-password") )
         wdata.ws_password = options.at("server-rpc-password").as<std::string>();

      fc::http::websocket_client client;
      idump((wdata.ws_server));
      auto con  = client.connect( wdata.ws_server );
      auto apic = std::make_shared<fc::rpc::websocket_api_connection>(*con);

      auto remote_api = apic->get_remote_api< login_api >(1);
      edump((wdata.ws_user)(wdata.ws_password) );
      // TODO:  Error message here
      FC_ASSERT( remote_api->login( wdata.ws_user, wdata.ws_password ) );

      auto wapiptr = std::make_shared<wallet_api>( wdata, remote_api );
      wapiptr->set_wallet_filename( wallet_file.generic_string() );
      wapiptr->load_wallet_file();

      fc::api<wallet_api> wapi(wapiptr);

      auto wallet_cli = std::make_shared<script_cli>();
      for( auto& name_formatter : wapiptr->get_result_formatters() )
         wallet_cli->format_result( name_formatter.first, name_formatter.second );

//      boost::signals2::scoped_connection closed_connection(con->closed.connect([=]{
//          std::cerr << "Server has disconnected us.\n";
//          wallet_cli->stop();
//      }));
//      (void)(closed_connection);


      auto _websocket_server = std::make_shared<fc::http::websocket_server>();
      if( options.count("rpc-endpoint") )
      {
         _websocket_server->on_connection([&]( const fc::http::websocket_connection_ptr& c ){
             std::cout << "here... \n";
             wlog("." );
             auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(*c);
             wsc->register_api(wapi);
             c->set_session_data( wsc );
         });
         ilog( "Listening for incoming RPC requests on ${p}", ("p", options.at("rpc-endpoint").as<string>() ));
         _websocket_server->listen( fc::ip::endpoint::from_string(options.at("rpc-endpoint").as<string>()) );
         _websocket_server->start_accept();
      }

      std::string cert_pem = "server.pem";
      if( options.count( "rpc-tls-certificate" ) )
         cert_pem = options.at("rpc-tls-certificate").as<std::string>();

      auto _websocket_tls_server = std::make_shared<fc::http::websocket_tls_server>(cert_pem);
      if( options.count("rpc-tls-endpoint") )
      {
         _websocket_tls_server->on_connection([&]( const fc::http::websocket_connection_ptr& c ){
             auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(*c);
             wsc->register_api(wapi);
             c->set_session_data( wsc );
         });
         ilog( "Listening for incoming TLS RPC requests on ${p}", ("p", options.at("rpc-tls-endpoint").as<std::string>() ));
         _websocket_tls_server->listen( fc::ip::endpoint::from_string(options.at("rpc-tls-endpoint").as<std::string>()) );
         _websocket_tls_server->start_accept();
      }

      auto _http_server = std::make_shared<fc::http::server>();
      if( options.count("rpc-http-endpoint" ) )
      {
         ilog( "Listening for incoming HTTP RPC requests on ${p}", ("p", options.at("rpc-http-endpoint").as<std::string>() ) );
         _http_server->listen( fc::ip::endpoint::from_string( options.at( "rpc-http-endpoint" ).as<std::string>() ) );
         //
         // due to implementation, on_request() must come AFTER listen()
         //
         _http_server->on_request(
               [&]( const fc::http::request& req, const fc::http::server::response& resp )
               {
                   std::shared_ptr< fc::rpc::http_api_connection > conn =
                         std::make_shared< fc::rpc::http_api_connection>();
                   conn->register_api( wapi );
                   conn->on_request( req, resp );
               } );
      }


      wallet_cli->register_api( wapi );
      wallet_cli->Initialize();


      std::string filename = "/Users/milanfranc/work/interpret/new_cmds.txt";

      DcScriptEngine engine;

      engine.open(filename);
      engine.set_wallet_api(wallet_cli);

      engine.interpret();


      wapi->save_wallet_file(wallet_file.generic_string());

   }
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
      return -1;
   }
   return 0;

}
