#include "../src/CommandTracking.h"
#include "../src/GroupMembership.h"
#include "../src/MuteNotice.h"
#include "../src/KillListFormat.h"
#undef NDEBUG
#include <cassert>
#include <iostream>

int main() {
    std::string largeRoster;
    for(int i = 0; i < 30; ++i) largeRoster += "Player" + std::to_string(i) + "          10   20   30   4000\n";
    const std::string largeBox = BuildKillListBox(largeRoster, 300, 600, 900, 120000);
    assert(largeBox.size() > 512);
    assert(largeBox.find("Player29") != std::string::npos && largeBox.find("120000") != std::string::npos);
    // Exact public chat line which caused a reconnect three seconds before GZ ended.
    assert(!IsOwnMuteNotice("_Ermes: (100) DONT SAY YALL MUTED ME"));
    assert(!IsOwnMuteNotice("Player(100): YOU ARE COMMAND MUTED FOR 60 SECONDS"));
    assert(!IsOwnMuteNotice("Player: (100) YOU ARE MUTED FOR 60 SECONDS"));
    assert(!IsOwnMuteNotice("(GROUP) [20] Player(100): YOU ARE MUTED"));
    assert(!IsOwnMuteNotice("ADMIN: X muted Player(100) for 60 minutes"));
    assert(!IsOwnMuteNotice("YOU ARE NOT MUTED"));
    assert(!IsOwnMuteNotice("YOU ARE MUTEDNESS"));
    assert(IsOwnMuteNotice("~g~YOU ARE COMMAND MUTED FOR 60 SECONDS"));
    assert(IsOwnMuteNotice("{FF0000}You are muted for 60 seconds."));
    assert(IsOwnMuteNotice("ERROR: You have been muted for 10 seconds."));
    assert(IsOwnMuteNotice("  ~r~YOU ARE~n~COMMAND MUTED FOR 60 SECONDS  "));
    assert(IsOwnGroupExit("GROUP: (Bot)(12) left the group.", "bot"));
    assert(IsOwnGroupExit("GROUP: Admin(5) kicked Bot(12) from the group.", "bot"));
    assert(IsOwnGroupExit("You have left the group.", "bot"));
    assert(IsOwnGroupExit("GROUP: You have left the group.", "bot"));
    assert(IsOwnGroupExit("Your group has been disbanded.", "bot"));
    assert(!IsOwnGroupExit("GROUP: Alice(2) left the group.", "bot"));
    assert(!IsOwnGroupExit("GROUP: Admin(5) kicked Alice(2) from the group.", "bot"));
    assert(!IsOwnGroupExit("Alice: You have left the group.", "bot"));
    assert(!IsOwnGroupExit("Alice: GROUP: Bot(12) left the group.", "bot"));
    assert(IsGroupLeaveDialog(true, 100, "UIF", "Are you sure you want to leave your group?"));
    assert(!IsGroupLeaveDialog(false, 100, "Leave Group", "Confirm"));
    assert(!IsGroupLeaveDialog(true, 20001, "Leave Group", "Confirm"));
    assert(!IsGroupLeaveDialog(true, 100, "Kick Group Member", "Confirm"));
    assert(GROUP_LEAVE_YES == 0 && GROUP_LEAVE_NO == 1);
    GroupCommandEvent event;
    // Actual DM invite from the log must not trigger group logging or ?post.
    assert(!GroupCommandEvent::parse(">> jugginpeace(58) invites you to his Deathmatch world! To join type '/dm 4'", event));
    assert(!GroupCommandEvent::parse("(GROUP) [20] Player(58): GROUP: washe(2845359) invited DarkLights.CRMM(3756473)", event));
    assert(!GroupCommandEvent::parse("GROUP: jugginpeace(58) invites you to his Deathmatch world!", event));
    assert(!GroupCommandEvent::parse("GROUP: washe(2845359) invited Unknown(0)", event));
    assert(GroupCommandEvent::parse("GROUP: washe(2845359) invited DarkLights.CRMM(3756473)", event));
    assert(event.type == "ginvite" && event.actor == "washe" && event.target == "DarkLights.CRMM");
    assert(GroupCommandEvent::parse("GROUP: (washe)(2845359) invited (DarkLights.CRMM)(3756473)", event));
    assert(GroupCommandEvent::parse("GROUP: (bot)(42) changed the role of Alice(5) from 'Member' to 'Co-Leader'", event));
    assert(event.type == "grole" && event.actor == "bot" && event.target == "Alice" && event.value == "Co-Leader");
    assert(GroupCommandEvent::parse("GROUP: bot(42) changed the rank of Alice(5) from 'Recruit' to 'Veteran'", event));
    assert(event.type == "grank" && event.value == "Veteran");
    assert(GroupCommandEvent::parse("GROUP: (bot)(42) kicked Alice(5) from the group", event));
    assert(event.type == "gkick" && event.actor == "bot" && event.target == "Alice");
    assert(GroupCommandEvent::parse("GROUP: bot(42) invited Alice(5) to the group", event));
    assert(event.type == "ginvite" && event.target == "Alice");
    assert(!GroupCommandEvent::parse("Someone: GROUP: bot(42) kicked Alice(5)", event));
    assert(!GroupCommandEvent::parse("ADMIN: bot(42) kicked Alice(5)", event));
    assert(!GroupCommandEvent::parse("GROUP: bot(42) changed the role of Alice(5)", event));
    PendingGroupCommand pending;
    assert(!pending.active());
    pending.requesterId = "request-1";
    pending.type = "grole";
    pending.target = "Alice";
    pending.value = "Co-Leader";
    pending.started = 100;
    assert(pending.matches("grole", "BOT", "bot", "alice", "co-leader"));
    assert(!pending.matches("grank", "BOT", "bot", "alice", "co-leader"));
    assert(!pending.matches("grole", "OTHER", "bot", "alice", "co-leader"));
    assert(!pending.matches("grole", "BOT", "bot", "bob", "co-leader"));
    assert(!pending.matches("grole", "BOT", "bot", "alice", "Member"));
    assert(!pending.matches("grole", "BOT", "", "alice", "co-leader"));
    assert(!pending.expired(60100));
    assert(pending.expired(60101));
    // Windows GetTickCount wraps every ~49 days.
    pending.started = 0xFFFFFF00UL;
    assert(!pending.expired(0x00000100UL));
    assert(pending.expired(0x00010000UL));
    pending.type = "ginvite";
    pending.value.clear();
    assert(pending.matches("ginvite", "BOT", "bot", "Alice", ""));
    pending = PendingGroupCommand();
    assert(!pending.matches("ginvite", "BOT", "bot", "Alice", ""));
    std::cout << "Command correlation: OK\n";
}
