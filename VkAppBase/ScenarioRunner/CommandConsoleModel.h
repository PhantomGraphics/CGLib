#pragma once
#include "CommandInfo.h"

// Compatibility facade; shared state lives in Editor, independent of ScenarioRunner.
#include "../../Editor/Console/CommandConsoleModel.h"

using CommandConsoleModel = Phantom::Editor::CommandConsoleModel;
