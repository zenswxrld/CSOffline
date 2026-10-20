#pragma once

#include <chrono>
#include <string>
#include <vector>

// Bots talk in the in-game chat: greetings, reactions to kills, the bomb and round and match results.
//
// BotChat::OnEvent turns what happened into a few short lines said by one of the bots (names are the real bot names),
// each one delayed like someone typing. Tick() prints the lines that are due through the server's own
// UTIL_ClientPrintAll (Windows only, found by its first bytes in server.dll), so they show in the chat like any other
// message. Only on this computer's game: a bot "says" it by us printing "<name>: <text>" in the chat.
//
// Each bot has a personality (see bot_personality.h): chatty ones talk a lot, quiet ones hardly ever, rushers trash talk,
// campers stay calm, friendly ones compliment. The bot that has killed you most in the match is your nemesis (see
// nemesis.h) and has lines of its own.
//
// Off unless "bot_chat" is on in config.txt.
namespace BotChat
{
    enum class EventType
    {
        Kill,
        RoundStart,
        RoundEnd,
        BombPlanted,
        BombDefused,
        BombExploded,
        MatchEnd,
        NemesisNamed, // attacker is the bot that became the local player's nemesis
        NemesisSlain, // attacker is the nemesis the local player just killed
    };

    struct Event
    {
        EventType type{};
        std::string victim;
        std::string attacker;
        bool victimBot{};
        bool attackerBot{};
        bool victimLocal{};
        bool attackerLocal{};
        bool headshot{};
        bool localWon{}; // RoundEnd and MatchEnd: the local player's team won
        int roundsPlayed{}; // RoundStart: 0 for the first round of the match
    };

    // the bots in the match (their names)
    void SetBots(std::vector<std::string> names);

    // a seed of 0 picks one; the same seed gives the same chat (for the tests)
    void Reset(unsigned seed = 0);

    // what happened; may queue a line
    void OnEvent(const Event &event, std::chrono::steady_clock::time_point now);

    // the lines due by now, formatted for the chat ("<name>: <text>" with colours)
    void Take(std::chrono::steady_clock::time_point now, std::vector<std::string> &lines);

    // plain text of a queued or finished line, for the tests: "name: text"
    std::string PlainText(const std::string &chatLine);

    // called every frame from the game's main thread: prints the lines that are due. no-op unless enabled
    void Tick();

    // the chat events the game listener passes on
    void OnGameEvent(const Event &event);
}
