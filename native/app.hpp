#pragma once
#include "catalog.hpp"
#include <windows.h>
#include <atomic>
#include <mutex>
namespace spirit {
struct App {
 std::mutex lock;Json catalog=nullptr,baseline=nullptr;std::string status="Ожидание входа в Sky…",diagnostic;
 std::wstring downloadDirectory,cacheDirectory;std::atomic<bool> ready{false},supported{false},busy{false},readRequested{false},exportRequested{false};
 std::atomic<unsigned> build{0};std::atomic<unsigned long long> revision{0};bool demo=false;
 std::atomic<bool> baselineRequested{false};
 std::atomic<bool> liveCatalog{false};
};
extern App app;
void log(const std::string& text);
void worker(HMODULE module);
void overlay(HMODULE module);
void drawPanel();
void saveJson(const std::wstring& path,const Json& value,bool backup);
}
