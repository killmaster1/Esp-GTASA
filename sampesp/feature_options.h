#pragma once
#include <atomic>
namespace options {
// Features start enabled; this build has no menu, text UI or input hooks.
std::atomic<bool> boxEnabled{true},lineEnabled{true},healthEnabled{true},aimEnabled{true},zoomEnabled{true},penetrationEnabled{true};
}
