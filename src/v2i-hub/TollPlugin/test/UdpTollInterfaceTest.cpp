#include <gtest/gtest.h>

#include "../include/UdpTollInterface.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>
#include <unistd.h>

namespace v2x { namespace toll { namespace test {

namespace {
int bindReceiver(uint16_t& portOut) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        close(sock);
        return -1;
    }
    portOut = ntohs(addr.sin_port);

    timeval timeout{};
    timeout.tv_sec = 1;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return sock;
}
}

TEST(UdpTollInterfaceTest, SendsTamJsonToConfiguredObeHostAndPort) {
    uint16_t port = 0;
    int recvSock = bindReceiver(port);
    ASSERT_GE(recvSock, 0);

    TollPluginConfig cfg;
    cfg.enableUdpInterface = true;
    cfg.obeHost = "127.0.0.1";
    cfg.tamUdpPort = port;
    cfg.tumUdpPort = 0;
    cfg.udpEncodingMode = "JSON";

    UdpTollInterface udp(nullptr);

    TamMessage tam;
    tam.set_msgType("TAM");
    tam.set_msgCount(7);
    tam.set_tollPointID("camden-nj-001");
    tam.set_tollChargerID("camden-rse-01");
    tam.set_broadcastIntervalMs(1000);

    udp.sendTam(tam, cfg);

    char buf[2048] = {0};
    int n = recv(recvSock, buf, sizeof(buf) - 1, 0);
    close(recvSock);

    ASSERT_GT(n, 0);
    std::string payload(buf, buf + n);
    EXPECT_NE(payload.find("\"msgType\":\"TAM\""), std::string::npos);
    EXPECT_NE(payload.find("\"msgCount\":\"7\""), std::string::npos);
    EXPECT_EQ(udp.tamSentCount(), 1);
    EXPECT_EQ(udp.errorCount(), 0);
}

TEST(UdpTollInterfaceTest, ReceivesTumJsonOnConfiguredPort) {
    std::mutex mutex;
    std::condition_variable cv;
    bool received = false;
    TumMessage captured;

    UdpTollInterface udp([&](TumMessage& tum) {
        std::lock_guard<std::mutex> lock(mutex);
        captured = tum;
        received = true;
        cv.notify_one();
    });

    TollPluginConfig cfg;
    cfg.enableUdpInterface = true;
    cfg.udpBindHost = "127.0.0.1";
    cfg.tumUdpPort = 20000 + (getpid() % 10000);
    cfg.udpEncodingMode = "JSON";
    udp.updateConfig(cfg);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(sock, 0);
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(static_cast<uint16_t>(cfg.tumUdpPort));
    ASSERT_EQ(inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr), 1);

    std::string tumJson =
        "{\"msgType\":\"TUM\",\"tumSequenceNum\":9,\"tamSequenceNum\":7,"
        "\"tempID\":\"AABB1234\",\"vehicleClass\":\"passenger_car\","
        "\"vehicleType\":610,\"tollPointID\":\"camden-nj-001\","
        "\"tollChargerID\":\"camden-rse-01\",\"laneID\":1,"
        "\"eventTimeUtc\":\"2026-06-08T18:00:00Z\"}";
    ASSERT_EQ(sendto(sock, tumJson.data(), tumJson.size(), 0,
                     reinterpret_cast<sockaddr*>(&dst), sizeof(dst)),
              static_cast<ssize_t>(tumJson.size()));
    close(sock);

    std::unique_lock<std::mutex> lock(mutex);
    bool gotTum = cv.wait_for(lock, std::chrono::seconds(2), [&] { return received; });
    lock.unlock();
    udp.stop();

    ASSERT_TRUE(gotTum);
    EXPECT_EQ(captured.get_msgType(), "TUM");
    EXPECT_EQ(captured.get_tumSequenceNum(), 9);
    EXPECT_EQ(captured.get_tamSequenceNum(), 7);
    EXPECT_EQ(captured.get_tempID(), "AABB1234");
    EXPECT_EQ(captured.get_vehicleClass(), "passenger_car");
    EXPECT_EQ(captured.get_laneID(), 1);
    EXPECT_EQ(udp.tumReceivedCount(), 1);

}

}}} // namespace v2x::toll::test
