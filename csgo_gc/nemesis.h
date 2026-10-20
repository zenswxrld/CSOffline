#pragma once

#include <string>

// The bot that keeps killing you becomes your nemesis. Once one bot has killed you three times in a match and has
// killed you more than any other bot, it is named your nemesis. Kill it and the nemesis is slain; it can be named
// again after three more deaths. A new match starts clean.
namespace Nemesis
{
    enum class Result
    {
        None,
        Named, // name is the new nemesis
        Slain, // name is the nemesis the local player just killed
    };

    constexpr int DeathsToBeNamed = 3;

    void Reset();

    // a kill in the match. names are the player names, the flags say who they are
    Result OnKill(const std::string &attacker, bool attackerBot, const std::string &victim, bool victimBot,
        bool victimLocal, bool attackerLocal, std::string &name);

    // empty if there is none
    std::string Current();
}
