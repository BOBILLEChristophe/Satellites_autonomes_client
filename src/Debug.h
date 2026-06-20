/*
  Debug.h
*/
#pragma once

#include <Arduino.h>
#include <cstring>

// --- Helper: basename portable (Linux/Mac/Windows paths) ---
static inline const char* baseName(const char* path) {
  const char* slash = strrchr(path, '/');
  const char* back  = strrchr(path, '\\');
  const char* p = (slash > back) ? slash : back;
  return p ? (p + 1) : path;
}

#define FILENAME (baseName(__FILE__))

// ----------- CONFIGURATION -----------
#ifndef DEBUG_ENABLED
  #define DEBUG_ENABLED 4   // 0 pour couper tous les logs
#endif

// Niveaux (plus grand = plus verbeux)
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_DEBUG  3
#define LOG_LEVEL_ERROR  4

#ifndef LOG_LEVEL
  #define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

// ----------- MACRO INTERNE UNIQUE -----------
#if DEBUG_ENABLED
  #define LOG_BASE(levelStr, fmt, ...) \
    do { \
      Serial.printf("[%s][%s:%d] " fmt "\n", \
                    levelStr, FILENAME, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
  #define LOG_BASE(levelStr, fmt, ...) do {} while (0)
#endif

// ----------- MACROS PUBLICS SELON NIVEAU -----------
#if DEBUG_ENABLED && (LOG_LEVEL >= LOG_LEVEL_INFO)
  #define LOG_INFO(fmt, ...)  LOG_BASE("INFO", fmt, ##__VA_ARGS__)
#else
  #define LOG_INFO(...)       do {} while (0)
#endif

#if DEBUG_ENABLED && (LOG_LEVEL >= LOG_LEVEL_WARN)
  #define LOG_WARN(fmt, ...)  LOG_BASE("WARN", fmt, ##__VA_ARGS__)
#else
  #define LOG_WARN(...)       do {} while (0)
#endif

#if DEBUG_ENABLED && (LOG_LEVEL >= LOG_LEVEL_ERROR)
  #define LOG_ERROR(fmt, ...) LOG_BASE("ERROR", fmt, ##__VA_ARGS__)
#else
  #define LOG_ERROR(...)      do {} while (0)
#endif

#if DEBUG_ENABLED && (LOG_LEVEL >= LOG_LEVEL_DEBUG)
  #define LOG_DEBUG(fmt, ...) LOG_BASE("DEBUG", fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(...)      do {} while (0)
#endif
