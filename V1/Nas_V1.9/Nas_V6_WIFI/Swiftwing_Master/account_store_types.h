#pragma once

#include <Arduino.h>

struct StoredNasUser {
  String username;
  String status;
  String saltHex;
  String passwordHashHex;
  bool valid;
};
