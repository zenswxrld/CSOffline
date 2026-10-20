#pragma once

#include <string>

// Every bot has a personality that shows in how much it talks and what it says, and in its name. It is worked out from
// the name alone, so the same name always gives the same personality: names with a hint in them ("RushB_Dave",
// "AngleHolder", "Yapper") get the matching one, every other name gets one from a hash of the name.
enum class BotPersonality
{
    Chatty, // talks all the time
    Quiet, // hardly says anything
    Rusher, // aggressive, trash talks
    Camper, // patient, holds angles
    Friendly, // compliments, gg
};

constexpr int BotPersonalityCount = 5;

BotPersonality PersonalityOfBot(const std::string &name);
const char *BotPersonalityName(BotPersonality personality);

// a name that hints at the personality, from a random number
std::string PersonalityHintName(BotPersonality personality, unsigned random);
