#pragma once
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <fstream>
#include <regex>
#include <windows.h>
#include "enet/include/enet.h"
#include "http.h"
#include "server.h"
#include "print.h"
#include "events.h"
#include "utils.h"
#include "proton/rtparam.hpp"
#include "HTTPRequest.hpp"

server* g_server = new server();

using namespace std;

// Handler to reset hosts file on Ctrl+C
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    print::set_text("\nFixing Hosts File!", LightGreen);

    try {
        std::ofstream hostsFile("C:\\Windows\\System32\\drivers\\etc\\hosts", std::ios::trunc);
        // Clears the hosts file safely
        hostsFile.close();
    }
    catch (...) {}

    return FALSE; // pass control to next handler
}

// Utility to split string
vector<string> split(const string& str, const string& delim)
{
    vector<string> tokens;
    size_t prev = 0, pos = 0;

    while ((pos = str.find(delim, prev)) != string::npos) {
        string token = str.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }

    string lastToken = str.substr(prev);
    if (!lastToken.empty()) tokens.push_back(lastToken);

    return tokens;
}

// Setup Growtopia server and hosts file
void setgtserver() {
    try {
        // Clear hosts file
        std::ofstream hostsFile("C:\\Windows\\System32\\drivers\\etc\\hosts", std::ios::trunc);
        hostsFile.close();
    } catch (...) {}

    try {
        http::Request request{ "http://a104-125-3-135.deploy.static.akamaitechnologies.com/growtopia/server_data.php" };
        const auto response = request.send("POST", "version=3.91&protocol=160&platform=0", { "Host: www.growtopia1.com" });
        rtvar var = rtvar::parse({ response.body.begin(), response.body.end() });
        var.serialize();

        if (var.find("server")) {
            g_server->m_port = var.get_int("port");
            g_server->portz = var.get_int("port");
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Request failed, error: " << e.what() << '\n';
    }

    try {
        // Write local proxy to hosts file
        std::ofstream hostsFile("C:\\Windows\\System32\\drivers\\etc\\hosts", std::ios::trunc);
        if (hostsFile.is_open()) {
            hostsFile <<
                "127.0.0.1 growtopia1.com\n"
                "127.0.0.1 growtopia2.com\n"
                "127.0.0.1 www.growtopia1.com\n"
                "127.0.0.1 www.growtopia2.com";
            hostsFile.close();
        }
    } catch (...) {}
}

int main() {
    cout << "Last Update at 25.01.2024. Full update available via SaveForwarder Discord: https://discord.gg/X56gwJdxkn" << endl;
    system("start https://discord.gg/X56gwJdxkn");

    SetConsoleTitleA("SrMotion Proxy ;)");
    printf("Parsing the server_data.php\n");

    SetConsoleCtrlHandler(HandlerRoutine, TRUE);
    setgtserver();

    system("Color a");
    printf("Based on enet by ama.\n");

    events::out::type2 = 2;

    g_server->ipserver = "127.0.0.1";
    g_server->create = "0.0.0.0";

    std::thread httpThread(http::run, g_server->ipserver, "17191");
    httpThread.detach();

    print::set_color(LightGreen);
    if (enet_initialize() != 0) {
        print::set_text("ENet initialization failed.\n", LightRed);
        return 1;
    }

    if (g_server->start()) {
        print::set_text("Server & client proxy is running.\n", LightGreen);
        while (true) {
            g_server->poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } else {
        print::set_text("Failed to start server or proxy.\n", LightRed);
    }

    return 0;
}
