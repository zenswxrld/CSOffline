#include "stdafx.h"
#include "bot_chat.h"
#include "bot_personality.h"
#include "config.h"
#include "platform.h"

#include <algorithm>
#include <deque>
#include <random>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

struct Line
{
    std::chrono::steady_clock::time_point due;
    std::string text;
};

std::mt19937 s_random{ 1 };
std::vector<std::string> s_bots;
std::deque<Line> s_queue;
std::chrono::steady_clock::time_point s_lastSpoke{};
bool s_spokeBefore;

// the colour codes the chat understands: \x0a is a neutral grey-blue for the name, \x01 white for the text
constexpr char NameColour[] = "\x01\x0b";
constexpr char TextColour[] = "\x01";

int Roll(int count)
{
    return static_cast<int>(s_random() % static_cast<unsigned>(std::max(count, 1)));
}

bool Chance(int percent)
{
    return Roll(100) < percent;
}

template <size_t N>
const char *Pick(const char *const (&lines)[N])
{
    return lines[Roll(static_cast<int>(N))];
}

// how much of the normal chance a personality talks, in percent
int Talkativeness(BotPersonality personality)
{
    switch (personality)
    {
    case BotPersonality::Chatty: return 190;
    case BotPersonality::Quiet: return 30;
    case BotPersonality::Rusher: return 110;
    case BotPersonality::Camper: return 70;
    case BotPersonality::Friendly: return 120;
    }
    return 100;
}

// how likely a bot is to be the one that speaks up when it doesn't matter which
int SpeakerWeight(BotPersonality personality)
{
    switch (personality)
    {
    case BotPersonality::Chatty: return 8;
    case BotPersonality::Quiet: return 1;
    case BotPersonality::Rusher: return 4;
    case BotPersonality::Camper: return 2;
    case BotPersonality::Friendly: return 4;
    }
    return 3;
}

bool ChanceFor(const std::string &bot, int percent)
{
    return Chance(percent * Talkativeness(PersonalityOfBot(bot)) / 100);
}

std::string PickBot(const std::string &preferred = {})
{
    if (!preferred.empty())
    {
        return preferred;
    }
    if (s_bots.empty())
    {
        return {};
    }

    int total = 0;
    for (const std::string &bot : s_bots)
    {
        total += SpeakerWeight(PersonalityOfBot(bot));
    }
    int roll = Roll(total);
    for (const std::string &bot : s_bots)
    {
        roll -= SpeakerWeight(PersonalityOfBot(bot));
        if (roll < 0)
        {
            return bot;
        }
    }
    return s_bots.back();
}

// what a bot says in a kind of moment: its personality's own lines most of the time, the common ones otherwise
enum class Moment
{
    Beaten, // killed by the local player
    BeatenHeadshot,
    Taunt, // killed the local player
    Hype, // a round starts
    Won, // the bots' side won a round
    Lost, // the bots' side lost a round
    Greeting,
    MatchOver,
};

const char *PersonalLine(BotPersonality personality, Moment moment)
{
    static const char *const RusherBeaten[] = { "lucky", "cheater", "whatever", "ill be back", "rematch rn", "pfft" };
    static const char *const RusherTaunt[] = { "ez", "sit down", "get rekt", "too slow", "owned", "clapped", "bye" };
    static const char *const RusherHype[] = { "rush b", "full send", "go go go", "no stopping", "everyone rush", "yolo" };
    static const char *const RusherWon[] = { "ez", "told you", "rekt", "rush works" };
    static const char *const RusherLost[] = { "rush again", "one more", "should have rushed", "pfft" };
    static const char *const RusherGreeting[] = { "lets rush", "ready to frag", "who first" };
    static const char *const RusherOver[] = { "ez", "gg ez", "rematch", "again" };

    static const char *const CamperBeaten[] = { "i was holding that", "you found me", "hm", "nice peek", "didnt hear you", "ok" };
    static const char *const CamperTaunt[] = { "i see you", "right where i thought", "patience", "stay back", "thanks for walking in" };
    static const char *const CamperHype[] = { "hold the angles", "stay put", "dont peek", "wait for them", "patience", "eco?" };
    static const char *const CamperWon[] = { "told you hold", "patience wins", "nice hold", "solid" };
    static const char *const CamperLost[] = { "should have waited", "dont peek", "too fast", "hm ok" };
    static const char *const CamperGreeting[] = { "hold angles", "gl", "patience" };
    static const char *const CamperOver[] = { "gg", "good hold", "wp" };

    static const char *const ChattyBeaten[] = { "wait what how did you even see me there", "ok that was actually a good shot", "i literally had that lol", "bruh i was just about to go", "lol i cant believe that" };
    static const char *const ChattyTaunt[] = { "haha called it", "that was so easy lol", "i knew you would be there", "lol did you see that", "ok that was fun" };
    static const char *const ChattyHype[] = { "ok this round we have to focus", "anyone want to eco", "i have a good feeling about this one", "ok ok ok lets go lets go", "wonder what they will do" };
    static const char *const ChattyWon[] = { "gg that round was so good", "haha did you see that", "we are doing great lets keep going" };
    static const char *const ChattyLost[] = { "ugh ok that was bad but we can get the next one", "wait what happened there", "i dont even know what went wrong" };
    static const char *const ChattyGreeting[] = { "hello everyone how are we all doing", "hey hey gl hf all", "hi hi lets have a good one" };
    static const char *const ChattyOver[] = { "gg that was a really fun game", "gg wp everyone that was great", "nice game all want to go again" };

    static const char *const QuietBeaten[] = { "." , "hm", "ok" };
    static const char *const QuietTaunt[] = { ".", "gg" };
    static const char *const QuietHype[] = { "ok", "hm" };
    static const char *const QuietWon[] = { "gg" };
    static const char *const QuietLost[] = { "ok", "hm" };
    static const char *const QuietGreeting[] = { "hi", "gl" };
    static const char *const QuietOver[] = { "gg" };

    static const char *const FriendlyBeaten[] = { "nice shot!", "wow good one", "well played", "great aim!", "gj" };
    static const char *const FriendlyTaunt[] = { "sorry!", "gg", "good try", "nice try", "close one" };
    static const char *const FriendlyHype[] = { "good luck everyone", "have fun", "lets do this together", "you got this" };
    static const char *const FriendlyWon[] = { "good round", "well played everyone", "nice one team", "gg wp" };
    static const char *const FriendlyLost[] = { "wp them", "good round", "no worries", "we will get the next one" };
    static const char *const FriendlyGreeting[] = { "hi friends!", "good luck have fun", "gl hf everyone", "hello hello" };
    static const char *const FriendlyOver[] = { "gg wp everyone", "that was fun", "thanks for the game", "good game friends" };

    const auto pick = [](auto &lines) { return Pick(lines); };
    switch (personality)
    {
    case BotPersonality::Rusher:
        switch (moment)
        {
        case Moment::Beaten:
        case Moment::BeatenHeadshot: return pick(RusherBeaten);
        case Moment::Taunt: return pick(RusherTaunt);
        case Moment::Hype: return pick(RusherHype);
        case Moment::Won: return pick(RusherWon);
        case Moment::Lost: return pick(RusherLost);
        case Moment::Greeting: return pick(RusherGreeting);
        case Moment::MatchOver: return pick(RusherOver);
        }
        break;
    case BotPersonality::Camper:
        switch (moment)
        {
        case Moment::Beaten:
        case Moment::BeatenHeadshot: return pick(CamperBeaten);
        case Moment::Taunt: return pick(CamperTaunt);
        case Moment::Hype: return pick(CamperHype);
        case Moment::Won: return pick(CamperWon);
        case Moment::Lost: return pick(CamperLost);
        case Moment::Greeting: return pick(CamperGreeting);
        case Moment::MatchOver: return pick(CamperOver);
        }
        break;
    case BotPersonality::Chatty:
        switch (moment)
        {
        case Moment::Beaten:
        case Moment::BeatenHeadshot: return pick(ChattyBeaten);
        case Moment::Taunt: return pick(ChattyTaunt);
        case Moment::Hype: return pick(ChattyHype);
        case Moment::Won: return pick(ChattyWon);
        case Moment::Lost: return pick(ChattyLost);
        case Moment::Greeting: return pick(ChattyGreeting);
        case Moment::MatchOver: return pick(ChattyOver);
        }
        break;
    case BotPersonality::Quiet:
        switch (moment)
        {
        case Moment::Beaten:
        case Moment::BeatenHeadshot: return pick(QuietBeaten);
        case Moment::Taunt: return pick(QuietTaunt);
        case Moment::Hype: return pick(QuietHype);
        case Moment::Won: return pick(QuietWon);
        case Moment::Lost: return pick(QuietLost);
        case Moment::Greeting: return pick(QuietGreeting);
        case Moment::MatchOver: return pick(QuietOver);
        }
        break;
    case BotPersonality::Friendly:
        switch (moment)
        {
        case Moment::Beaten:
        case Moment::BeatenHeadshot: return pick(FriendlyBeaten);
        case Moment::Taunt: return pick(FriendlyTaunt);
        case Moment::Hype: return pick(FriendlyHype);
        case Moment::Won: return pick(FriendlyWon);
        case Moment::Lost: return pick(FriendlyLost);
        case Moment::Greeting: return pick(FriendlyGreeting);
        case Moment::MatchOver: return pick(FriendlyOver);
        }
        break;
    }
    return "gg";
}

const char *CommonLine(Moment moment)
{
    static const char *const Beaten[] = { "wtf", "nice shot", "how", "lol ok", "damn", "ouch", "nt", "bruh", "ez for you" };
    static const char *const Headshot[] = { "nice aim", "headshot lol", "hacks?", "wow ok", "crazy aim" };
    static const char *const Taunts[] = { "ez", "sit", "lol", "too easy", "nt", "gg", "rip", "haha", "ez clap" };
    static const char *const Hype[] = { "lets go", "this round", "focus", "ok ok", "we got this", "eco?" };
    static const char *const Lost[] = { "gg", "wp", "ugh", "ok ok", "nice round", "next one", "damn" };
    static const char *const Won[] = { "ez", "gg", "nice", "lets go", "too easy", "gg ez" };
    static const char *const Greetings[] = { "gl hf", "glhf", "hi all", "hey gl", "lets go", "gl" };
    static const char *const Over[] = { "gg", "gg wp", "wp", "gg all", "nice game", "gg well played" };
    switch (moment)
    {
    case Moment::Beaten: return Pick(Beaten);
    case Moment::BeatenHeadshot: return Pick(Headshot);
    case Moment::Taunt: return Pick(Taunts);
    case Moment::Hype: return Pick(Hype);
    case Moment::Won: return Pick(Won);
    case Moment::Lost: return Pick(Lost);
    case Moment::Greeting: return Pick(Greetings);
    case Moment::MatchOver: return Pick(Over);
    }
    return "gg";
}

// its own personality's line about two times in three
std::string LineFor(const std::string &bot, Moment moment)
{
    return Chance(65) ? PersonalLine(PersonalityOfBot(bot), moment) : CommonLine(moment);
}

std::string s_nemesis;
// bots the local player killed this round, and the ones killed in the round before (they want revenge)
std::vector<std::string> s_killedThisRound;
std::vector<std::string> s_wantRevenge;

bool Contains(const std::vector<std::string> &list, const std::string &name)
{
    return std::find(list.begin(), list.end(), name) != list.end();
}

std::string WithName(const char *pattern, const std::string &name)
{
    std::string line = pattern;
    const size_t at = line.find("{n}");
    if (at != std::string::npos)
    {
        line.replace(at, 3, name);
    }
    return line;
}


std::string Format(const std::string &bot, const std::string &text)
{
    return std::string{ " " } + NameColour + bot + TextColour + ": " + text;
}

void Say(const std::string &bot, const std::string &text, std::chrono::steady_clock::time_point now, int minMs = 1200,
    int maxMs = 3800, bool force = false)
{
    if (bot.empty() || s_queue.size() >= 4)
    {
        return;
    }

    // never two lines at once, and a pause after anything said
    constexpr auto Gap = std::chrono::milliseconds{ 4500 };
    if (!force)
    {
        if (s_spokeBefore && now - s_lastSpoke < Gap)
        {
            return;
        }
        for (const Line &queued : s_queue)
        {
            if (queued.due - now < Gap)
            {
                return;
            }
        }
    }

    // typing takes longer for a longer line
    const int typing = static_cast<int>(text.size()) * 55;
    const int delay = minMs + Roll(std::max(1, maxMs - minMs)) + typing;
    s_queue.push_back({ now + std::chrono::milliseconds{ delay }, Format(bot, text) });
}

// two bots talk to each other: one speaks, the other answers a little later. false if there aren't two bots
bool Banter(const char *const *openers, size_t openerCount, const char *const *answers, size_t answerCount,
    std::chrono::steady_clock::time_point now)
{
    if (s_bots.size() < 2)
    {
        return false;
    }

    const std::string first = PickBot();
    std::string second = PickBot();
    for (int tries = 0; tries < 8 && second == first; ++tries)
    {
        second = PickBot();
    }
    if (second == first || !s_queue.empty())
    {
        return false;
    }

    Say(first, WithName(openers[Roll(static_cast<int>(openerCount))], second), now, 900, 2400, true);
    Say(second, answers[Roll(static_cast<int>(answerCount))], now, 3000, 5000, true);
    return true;
}

} // namespace

void BotChat::SetBots(std::vector<std::string> names)
{
    s_bots = std::move(names);
}

void BotChat::Reset(unsigned seed)
{
    s_random.seed(seed ? seed : std::random_device{}());
    s_queue.clear();
    s_bots.clear();
    s_nemesis.clear();
    s_killedThisRound.clear();
    s_wantRevenge.clear();
    s_spokeBefore = false;
}

void BotChat::OnEvent(const Event &event, std::chrono::steady_clock::time_point now)
{
    if (s_bots.empty())
    {
        return;
    }

    switch (event.type)
    {
    case EventType::Kill:
        if (event.victimBot && event.attackerLocal)
        {
            // you killed a bot
            if (!s_nemesis.empty() && event.victim == s_nemesis)
            {
                break; // the nemesis has its own line
            }
            if (!Contains(s_killedThisRound, event.victim))
            {
                s_killedThisRound.push_back(event.victim);
            }
            if (ChanceFor(event.victim, event.headshot ? 45 : 22))
            {
                Say(event.victim, LineFor(event.victim, event.headshot ? Moment::BeatenHeadshot : Moment::Beaten), now);
            }
            else if (s_bots.size() > 1 && Chance(10))
            {
                // another bot has something to say about it
                static const char *const Teasing[] = { "{n} that was bad", "lol {n}", "{n} you good?", "{n} how", "rip {n}" };
                std::string teaser = PickBot();
                for (int tries = 0; tries < 8 && teaser == event.victim; ++tries)
                {
                    teaser = PickBot();
                }
                if (teaser != event.victim)
                {
                    Say(teaser, WithName(Pick(Teasing), event.victim), now);
                }
            }
        }
        else if (event.attackerBot && event.victimLocal)
        {
            // a bot killed you; the nemesis gloats far more often
            static const char *const NemesisTaunts[] = { "again lol", "you again", "my favourite target", "back for more?",
                "i own you", "predictable", "thanks for the frag" };
            if (!s_nemesis.empty() && event.attacker == s_nemesis && Chance(70))
            {
                Say(event.attacker, Pick(NemesisTaunts), now, 900, 2200, true);
            }
            else if (Contains(s_wantRevenge, event.attacker) && Chance(75))
            {
                // killed by the local player last round, and back for it
                static const char *const Revenge[] = { "back for revenge", "remember me?", "payback time", "told you ill be back",
                    "that was for last round", "got you this time" };
                s_wantRevenge.erase(std::remove(s_wantRevenge.begin(), s_wantRevenge.end(), event.attacker), s_wantRevenge.end());
                Say(event.attacker, Pick(Revenge), now, 900, 2400, true);
            }
            else if (event.headshot && ChanceFor(event.attacker, 55))
            {
                static const char *const Headshots[] = { "headshot", "dome", "one tap", "right between the eyes", "nice try",
                    "tapped", "boom headshot" };
                Say(event.attacker, Pick(Headshots), now, 900, 2400);
            }
            else if (ChanceFor(event.attacker, 30))
            {
                Say(event.attacker, LineFor(event.attacker, Moment::Taunt), now);
            }
        }
        else if (event.attackerBot && event.victimBot)
        {
            static const char *const Between[] = { "rip", "nice", "gj", "lol", "oof", "my bad" };
            const std::string &speaker = Chance(50) ? event.victim : event.attacker;
            if (ChanceFor(speaker, 8))
            {
                Say(speaker, Pick(Between), now);
            }
        }
        break;

    case EventType::RoundStart:
    {
        // the bots you killed last round remember it; a new match starts clean
        s_wantRevenge = event.roundsPlayed == 0 ? std::vector<std::string>{} : s_killedThisRound;
        s_killedThisRound.clear();
        if (event.roundsPlayed == 0)
        {
            const std::string bot = PickBot();
            Say(bot, LineFor(bot, Moment::Greeting), now, 600, 2200);
        }
        else
        {
            const std::string bot = PickBot();
            if (ChanceFor(bot, 6))
            {
                Say(bot, LineFor(bot, Moment::Hype), now);
            }
        }
        break;
    }

    case EventType::RoundEnd:
    {
        // bots talk to each other: blame after a lost round, praise after a won one
        static const char *const Blame[] = { "{n} what was that", "{n} you were supposed to hold", "{n} wake up", "ugh {n}",
            "{n} seriously?", "thanks {n}", "{n} where were you" };
        static const char *const BlameAnswers[] = { "me?? you were the one who left", "lol shut up", "my bad", "sure blame me",
            "wasnt me", "i was watching the other site", "chill" };
        static const char *const Praise[] = { "nice one {n}", "{n} carried", "gj {n}", "{n} what a round", "wp {n}" };
        static const char *const PraiseAnswers[] = { "ty", "np", "ez", "we did it", "thanks!", "all of us" };
        if (Chance(event.localWon ? 18 : 14) && Banter(event.localWon ? Blame : Praise,
                event.localWon ? std::size(Blame) : std::size(Praise), event.localWon ? BlameAnswers : PraiseAnswers,
                event.localWon ? std::size(BlameAnswers) : std::size(PraiseAnswers), now))
        {
            break;
        }

        const std::string bot = PickBot();
        if (ChanceFor(bot, 22))
        {
            Say(bot, LineFor(bot, event.localWon ? Moment::Lost : Moment::Won), now);
        }
        break;
    }

    case EventType::BombPlanted:
    {
        static const char *const Planted[] = { "bomb down", "bomb planted", "its down", "plant!", "rotate" };
        const std::string bot = PickBot();
        if (ChanceFor(bot, 30))
        {
            Say(bot, Pick(Planted), now);
        }
        break;
    }

    case EventType::BombDefused:
    {
        static const char *const Defused[] = { "wp", "close one", "nice defuse", "ugh", "damn", "gg" };
        const std::string bot = PickBot();
        if (ChanceFor(bot, 35))
        {
            Say(bot, Pick(Defused), now);
        }
        break;
    }

    case EventType::BombExploded:
    {
        static const char *const Boom[] = { "gg", "boom", "nice", "wp" };
        const std::string bot = PickBot();
        if (ChanceFor(bot, 25))
        {
            Say(bot, Pick(Boom), now);
        }
        break;
    }

    case EventType::MatchEnd:
    {
        static const char *const Again[] = { "rematch?", "again?", "one more", "gg ez" };
        s_queue.clear(); // the end of the match always gets a word
        s_nemesis.clear();
        const std::string first = PickBot();
        Say(first, LineFor(first, Moment::MatchOver), now, 800, 2500, true);
        if (s_bots.size() > 1 && Chance(55))
        {
            std::string other = PickBot();
            for (int tries = 0; tries < 8 && other == first; ++tries)
            {
                other = PickBot();
            }
            if (other != first)
            {
                Say(other, Chance(60) ? LineFor(other, Moment::MatchOver) : std::string{ Pick(Again) }, now, 2600, 4500, true);
            }
        }
        break;
    }

    case EventType::NemesisNamed:
    {
        static const char *const Named[] = { "i think i found my nemesis", "you keep walking into me", "i got your number now",
            "this is becoming a habit", "ill keep an eye on you", "we meet again" };
        s_nemesis = event.attacker;
        Say(event.attacker, Pick(Named), now, 1200, 2600, true);
        break;
    }

    case EventType::NemesisSlain:
    {
        static const char *const Slain[] = { "ok that one was on me", "fine, you got me", "revenge...", "i will be back",
            "well played", "dont get cocky", "ill remember this" };
        s_nemesis.clear();
        Say(event.attacker, Pick(Slain), now, 900, 2200, true);
        break;
    }
    }
}

void BotChat::Take(std::chrono::steady_clock::time_point now, std::vector<std::string> &lines)
{
    while (!s_queue.empty() && s_queue.front().due <= now)
    {
        lines.push_back(std::move(s_queue.front().text));
        s_queue.pop_front();
        s_lastSpoke = now;
        s_spokeBefore = true;
    }
}

std::string BotChat::PlainText(const std::string &chatLine)
{
    std::string plain;
    for (char c : chatLine)
    {
        if (static_cast<unsigned char>(c) >= 0x20)
        {
            plain += c;
        }
    }
    const size_t start = plain.find_first_not_of(' ');
    return start == std::string::npos ? std::string{} : plain.substr(start);
}

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)

namespace
{

// UTIL_ClientPrintAll in the final CS:GO server.dll: msg_dest in ecx, the message in edx, three more string arguments
// on the stack (the caller removes them). Found by its first bytes, which are unique in the builds seen.
constexpr int16_t Pattern[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec, 0x20, 0x56, 0x57, 0x8b, 0xf9, 0x8b, 0xf2, 0x8d, 0x4c, 0x24, 0x08,
    0xe8, -1, -1, -1, -1, // call the broadcast filter's constructor
    0x51, 0xff, 0x75, 0x10, 0x8b, 0xd7, 0x8d, 0x4c, 0x24, 0x10, 0xff, 0x75, 0x0c, 0xff, 0x75, 0x08, 0x56,
    0xe8 // call UTIL_ClientPrintFilter
};

constexpr int HudPrintTalk = 3;

const uint8_t *s_clientPrintAll;
bool s_searched;

const uint8_t *FindPrintAll()
{
    if (s_searched)
    {
        return s_clientPrintAll;
    }

    HMODULE server = GetModuleHandleA("server.dll");
    if (!server)
    {
        return nullptr; // not loaded yet, try again
    }
    s_searched = true;

    auto base = reinterpret_cast<const uint8_t *>(server);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return nullptr;
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    auto section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
    {
        if (memcmp(section->Name, ".text", 5))
        {
            continue;
        }

        const uint8_t *begin = base + section->VirtualAddress;
        const size_t size = section->Misc.VirtualSize;
        const size_t length = sizeof(Pattern) / sizeof(Pattern[0]);
        const uint8_t *found = nullptr;
        int matches = 0;
        for (size_t at = 0; at + length <= size; ++at)
        {
            size_t k = 0;
            while (k < length && (Pattern[k] < 0 || begin[at + k] == Pattern[k]))
            {
                ++k;
            }
            if (k == length)
            {
                found = begin + at;
                ++matches;
            }
        }
        if (matches == 1)
        {
            s_clientPrintAll = found;
        }
        break;
    }

    Platform::Print(s_clientPrintAll ? "Bot chat: found the server's chat print function\n"
                                     : "Bot chat: this server.dll build is not supported, bots stay quiet\n");
    return s_clientPrintAll;
}

void PrintToChat(const std::string &text)
{
    const uint8_t *function = FindPrintAll();
    if (!function)
    {
        return;
    }

    const char *message = text.c_str();
    const int destination = HudPrintTalk;
    __asm
    {
        push 0
        push 0
        push 0
        mov edx, message
        mov ecx, destination
        mov eax, function
        call eax
        add esp, 0Ch
    }
}

} // namespace

void BotChat::Tick()
{
    if (s_queue.empty() || !GetConfig().BotChat().enabled)
    {
        return;
    }

    std::vector<std::string> lines;
    Take(std::chrono::steady_clock::now(), lines);
    for (const std::string &line : lines)
    {
        PrintToChat(line);
    }
}

#else

void BotChat::Tick()
{
}

#endif

void BotChat::OnGameEvent(const Event &event)
{
    if (!GetConfig().BotChat().enabled)
    {
        return;
    }
    OnEvent(event, std::chrono::steady_clock::now());
}
