#include "stdafx.h"
#include "bot_chat.h"
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

std::string PickBot(const std::string &preferred = {})
{
    if (!preferred.empty())
    {
        return preferred;
    }
    return s_bots.empty() ? std::string{} : s_bots[Roll(static_cast<int>(s_bots.size()))];
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
            static const char *const Beaten[] = { "wtf", "nice shot", "how", "lol ok", "damn", "ouch", "nt", "bruh", "ez for you" };
            static const char *const Headshot[] = { "nice aim", "headshot lol", "hacks?", "wow ok", "crazy aim" };
            if (Chance(event.headshot ? 45 : 22))
            {
                Say(event.victim, event.headshot ? Pick(Headshot) : Pick(Beaten), now);
            }
        }
        else if (event.attackerBot && event.victimLocal)
        {
            // a bot killed you
            static const char *const Taunts[] = { "ez", "sit", "lol", "too easy", "nt", "gg", "rip", "haha", "ez clap" };
            if (Chance(30))
            {
                Say(event.attacker, Pick(Taunts), now);
            }
        }
        else if (event.attackerBot && event.victimBot)
        {
            static const char *const Between[] = { "rip", "nice", "gj", "lol", "oof", "my bad" };
            if (Chance(8))
            {
                Say(Chance(50) ? event.victim : event.attacker, Pick(Between), now);
            }
        }
        break;

    case EventType::RoundStart:
        if (event.roundsPlayed == 0)
        {
            static const char *const Greetings[] = { "gl hf", "glhf", "hi all", "hey gl", "lets go", "gl" };
            Say(PickBot(), Pick(Greetings), now, 600, 2200);
        }
        else if (Chance(6))
        {
            static const char *const Hype[] = { "lets go", "this round", "focus", "ok ok", "we got this", "eco?" };
            Say(PickBot(), Pick(Hype), now);
        }
        break;

    case EventType::RoundEnd:
        if (event.localWon)
        {
            static const char *const Lost[] = { "gg", "wp", "ugh", "ok ok", "nice round", "next one", "damn" };
            if (Chance(22))
            {
                Say(PickBot(), Pick(Lost), now);
            }
        }
        else if (Chance(22))
        {
            static const char *const Won[] = { "ez", "gg", "nice", "lets go", "too easy", "gg ez" };
            Say(PickBot(), Pick(Won), now);
        }
        break;

    case EventType::BombPlanted:
        if (Chance(30))
        {
            static const char *const Planted[] = { "bomb down", "bomb planted", "its down", "plant!", "rotate" };
            Say(PickBot(), Pick(Planted), now);
        }
        break;

    case EventType::BombDefused:
        if (Chance(35))
        {
            static const char *const Defused[] = { "wp", "close one", "nice defuse", "ugh", "damn", "gg" };
            Say(PickBot(), Pick(Defused), now);
        }
        break;

    case EventType::BombExploded:
        if (Chance(25))
        {
            static const char *const Boom[] = { "gg", "boom", "nice", "wp" };
            Say(PickBot(), Pick(Boom), now);
        }
        break;

    case EventType::MatchEnd:
    {
        static const char *const Over[] = { "gg", "gg wp", "wp", "gg all", "nice game", "gg well played" };
        static const char *const Again[] = { "rematch?", "again?", "one more", "gg ez" };
        s_queue.clear(); // the end of the match always gets a word
        const std::string first = PickBot();
        Say(first, Pick(Over), now, 800, 2500, true);
        if (s_bots.size() > 1 && Chance(55))
        {
            std::string other = PickBot();
            for (int tries = 0; tries < 8 && other == first; ++tries)
            {
                other = PickBot();
            }
            if (other != first)
            {
                Say(other, Chance(60) ? Pick(Over) : Pick(Again), now, 2600, 4500, true);
            }
        }
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
