// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// Umbrella header for lib_command — the project-agnostic application command
// layer (envelope + queue + tracker + the source/result vocabulary). Include
// this from a project's command layer; layer project command enums + dispatchers
// on top. Depends only on ungula::core::util::Queue — no lib_node / UI / transport.

#include "ungula/command/command_types.h"
#include "ungula/command/command_envelope.h"
#include "ungula/command/command_ingress.h"
#include "ungula/command/command_queue.h"
#include "ungula/command/command_tracker.h"
