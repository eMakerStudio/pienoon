// Copyright 2014 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "precompiled.h"
#include "gpg_manager.h"

namespace fpl {

bool GPGManager::Initialize() {
  /*
  // This code is here because we may be able to do this part of the
  // initialization here in the future, rather than relying on JNI_OnLoad below.
  auto env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  JavaVM *vm = nullptr;
  auto ret = env->GetJavaVM(&vm);
  assert(ret >= 0);
  gpg::AndroidInitialization::JNI_OnLoad(vm);
  */
  gpg::AndroidPlatformConfiguration platform_configuration;
  platform_configuration.SetActivity((jobject)SDL_AndroidGetActivity());

  // Creates a games_services object that has lambda callbacks.
  state = kStart;
  game_services_ =
    gpg::GameServices::Builder()
      .SetDefaultOnLog(gpg::LogLevel::VERBOSE)
      .SetOnAuthActionStarted([this](gpg::AuthOperation op) {
        state = state == kAuthUILaunched
                ? kAuthUIStarted
                : kAutoAuthStarted;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "GPG: Auto Sign in started!");
      })
      .SetOnAuthActionFinished([this](gpg::AuthOperation op,
                                      gpg::AuthStatus status) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "GPG: Auto Sign in finished with a result of %d", status);
        state = status == gpg::AuthStatus::VALID
                ? kAuthed
                : (state == kAuthUIStarted ? kAuthUIFailed : kAutoAuthFailed);
      })
      .Create(platform_configuration);

  if (!game_services_) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                "GPG: failed to create GameServices!");
    return false;
  }

  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "GPG: created GameServices");
  return true;
}

// Called every frame from the game, to see if there's anything to be done
// with the async progress from gpg
void GPGManager::Update() {
  assert(game_services_);
  switch(state) {
    case kStart:
    case kAutoAuthStarted:
      // Nothing to do, waiting.
      break;
    case kAutoAuthFailed:
      // Need to explicitly ask for user  login.
      game_services_->StartAuthorizationUI();
      state = kAuthUILaunched;
      break;
    case kAuthUILaunched:
    case kAuthUIStarted:
      // Nothing to do, waiting.
      break;
    case kAuthUIFailed:
      // Both auto and UI based auth failed, I guess at this point we give up.
      break;
    case kAuthed:
      // We're good. TODO: Now start actually using gpg functionality...
      break;
  }
}

bool GPGManager::LoggedIn() {
  assert(game_services_);
  if (state < kAuthed) {
    SDL_LogWarn(SDL_LOG_CATEGORY_ERROR,
                "GPG: player not logged in, can\'t interact with gpg!");
    return false;
  }
  return true;
}


void GPGManager::SaveStat(const char *stat_id, uint64_t score) {
  if (!LoggedIn()) return;
  game_services_->Leaderboards().SubmitScore(stat_id, score);
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
              "GPG: submitted score %llu for id %s", score, stat_id);
}

void GPGManager::ShowLeaderboards() {
  if (!LoggedIn()) return;
  game_services_->Leaderboards().ShowAllUI();
}

}  // fpl

#ifdef __ANDROID__
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "main: JNI_OnLoad called");

  gpg::AndroidInitialization::JNI_OnLoad(vm);

  return JNI_VERSION_1_4;
}
#endif
