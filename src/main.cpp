#include "../lib/Simple-Web-Server/server_http.hpp"

#include <fstream>
#include <boost/filesystem.hpp>

using namespace std;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

void serveFile(string pathString, HttpServer::Response& response) {
    boost::filesystem::path path(pathString);
    if(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)) {
        ifstream ifs;
        ifs.open(path.string(), ifstream::in | ios::binary);

        if(ifs) {
            ifs.seekg(0, ios::end);
            size_t length=ifs.tellg();

            ifs.seekg(0, ios::beg);

            response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";

            //read and send 128 KB at a time
            size_t buffer_size=131072;
            vector<char> buffer;
            buffer.reserve(buffer_size);
            size_t read_length;
            try {
                while((read_length=ifs.read(&buffer[0], buffer_size).gcount())>0) {
                    response.write(&buffer[0], read_length);
                    response.flush();
                }
            } catch(const exception &e) {
                cerr << "Connection interrupted, closing file" << endl;
            }

            ifs.close();
            return;
        }
    }

    string content="Could not open path " + pathString;
    response << "HTTP/1.1 404 Not Found\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}

static boost::filesystem::path homeDir(getenv("HOME"));

int main() {
    HttpServer server(8080, 4);

    server.resource["^/api/storage/\\+(.+)$"]["GET"] = [](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        boost::filesystem::path requestPath(request->path_match[1]);
        string path = boost::filesystem::absolute(requestPath, homeDir).string();
        serveFile(path, response);
    };
    
    server.resource["^/api/storage/\\+(.+)$"]["POST"] = [](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        boost::filesystem::path requestPath(request->path_match[1]);
        string path = boost::filesystem::absolute(requestPath, homeDir).string();

        istream& data = request->content;

        ofstream ofs;
        ofs.open(path, ofstream::out | ios::binary);
        
        bool success = false;

        if(ofs) {
            size_t buffer_size = 131072;
            vector<char> buffer;
            buffer.reserve(buffer_size);
            size_t read_length;
            try {
                while((read_length = data.read(&buffer[0], buffer_size).gcount()) > 0) {
                    ofs.write(&buffer[0], read_length);
                    ofs.flush();
                }
                success = true;
            } catch(const exception& e) {
                cerr << "Connection interrupted, closing file" << endl;
            }

            ofs.close();
        }

        string resp;
        if(success) {
            resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        } else {
            resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        }
        response << resp;
    };

    server.default_resource["GET"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        boost::filesystem::path web_root_path("../webapp");
        auto path = web_root_path;
        path += request->path;
        if(boost::filesystem::exists(path)) {
            if(boost::filesystem::is_directory(path)) {
                path += "/index.html";
            }
        }

        if(!boost::filesystem::exists(path) || boost::filesystem::canonical(web_root_path) <= boost::filesystem::canonical(path)) {
            serveFile(path.string(), response);
        }
    };

    thread server_thread([&server]() {
        server.start();
    });

    server_thread.join();
}
