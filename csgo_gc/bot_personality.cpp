#include "stdafx.h"
#include "bot_personality.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace
{

struct Hint
{
    const char *text;
    BotPersonality personality;
};

// checked in this order, on the lower case name
constexpr Hint Hints[] = {
    { "yap", BotPersonality::Chatty }, { "chat", BotPersonality::Chatty }, { "mouth", BotPersonality::Chatty },
    { "talk", BotPersonality::Chatty }, { "typist", BotPersonality::Chatty }, { "textwall", BotPersonality::Chatty },
    { "silent", BotPersonality::Quiet }, { "mute", BotPersonality::Quiet }, { "shh", BotPersonality::Quiet },
    { "quiet", BotPersonality::Quiet }, { "nocomm", BotPersonality::Quiet }, { "whisper", BotPersonality::Quiet },
    { "rush", BotPersonality::Rusher }, { "entry", BotPersonality::Rusher }, { "aggro", BotPersonality::Rusher },
    { "kamikaze", BotPersonality::Rusher }, { "fragger", BotPersonality::Rusher },
    { "camp", BotPersonality::Camper }, { "anchor", BotPersonality::Camper }, { "angle", BotPersonality::Camper },
    { "lurk", BotPersonality::Camper }, { "hideout", BotPersonality::Camper },
    { "goodvibes", BotPersonality::Friendly }, { "nice", BotPersonality::Friendly }, { "buddy", BotPersonality::Friendly },
    { "friend", BotPersonality::Friendly }, { "teamplayer", BotPersonality::Friendly }, { "ggwp", BotPersonality::Friendly },
};

const char *const RusherNames[] = { "RushB", "Rush_B", "EntryFrag", "Aggro", "Kamikaze", "RushFull", "RusherX", "Fragger" };
const char *const CamperNames[] = { "Camper", "AngleHolder", "Anchor", "Lurker", "Hideout", "CampSite", "AnchorMan", "DeepLurk" };
const char *const ChattyNames[] = { "Yapper", "ChattyCat", "BigMouth", "TalkALot", "Typist", "YapYap", "TextWall", "Chatterbox" };
const char *const QuietNames[] = { "Silent", "MuteMic", "ShhNoComms", "Quietly", "NoComms", "Whisper", "SilentAim", "Mute_Hush" };
const char *const FriendlyNames[] = { "NiceGuy", "GoodVibes", "GGWP", "Buddy", "Friendly", "TeamPlayer", "NiceTry", "BestBuddy" };

template <size_t N>
const char *Choose(const char *const (&names)[N], unsigned random)
{
    return names[random % N];
}

} // namespace

BotPersonality PersonalityOfBot(const std::string &name)
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const Hint &hint : Hints)
    {
        if (lower.find(hint.text) != std::string::npos)
        {
            return hint.personality;
        }
    }

    // FNV-1a
    uint32_t hash = 2166136261u;
    for (unsigned char c : lower)
    {
        hash = (hash ^ c) * 16777619u;
    }
    return static_cast<BotPersonality>((hash >> 8) % BotPersonalityCount);
}

const char *BotPersonalityName(BotPersonality personality)
{
    switch (personality)
    {
    case BotPersonality::Chatty: return "chatty";
    case BotPersonality::Quiet: return "quiet";
    case BotPersonality::Rusher: return "rusher";
    case BotPersonality::Camper: return "camper";
    case BotPersonality::Friendly: return "friendly";
    }
    return "";
}

std::string PersonalityHintName(BotPersonality personality, unsigned random)
{
    std::string name;
    switch (personality)
    {
    case BotPersonality::Chatty: name = Choose(ChattyNames, random); break;
    case BotPersonality::Quiet: name = Choose(QuietNames, random); break;
    case BotPersonality::Rusher: name = Choose(RusherNames, random); break;
    case BotPersonality::Camper: name = Choose(CamperNames, random); break;
    case BotPersonality::Friendly: name = Choose(FriendlyNames, random); break;
    }

    // a number or a short suffix so the same hint can appear for several bots
    const unsigned extra = random / 7;
    switch (extra % 3)
    {
    case 0: name += "_" + std::to_string(extra % 90 + 10); break;
    case 1: name += std::to_string(extra % 900 + 100); break;
    default: break;
    }
    return name;
}
