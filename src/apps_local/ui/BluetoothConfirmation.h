#pragma once
#include "activities/util/ConfirmationActivity.h"

// Confirmations keep the link alive while taking ownership of all input.
class BluetoothConfirmation final : public ConfirmationActivity {
 public:
  using ConfirmationActivity::ConfirmationActivity;
  bool preservesRemoteConnection() const override { return true; }
  bool allowsRemoteControl() const override { return false; }
};
