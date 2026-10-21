# Connect Telegram

Use a fresh personal bot for Daybook. Setup runs on your Mac; afterward the Kindle talks to Telegram directly over HTTPS while the dashboard is open and connected to Wi-Fi.

1. In Telegram, open the verified **@BotFather**, send `/newbot`, and follow its prompts.
2. Keep the token private. Do not paste it into GitHub or this chat.
3. Install Daybook, then connect the Kindle by USB while Daybook is closed.
4. Run `python3 scripts/setup-telegram.py --data /Volumes/Kindle/daybook/data --mac-dialog`.
5. Paste the token into the hidden prompt. Send the generated pairing command to your new bot in a private chat.
6. Safely eject, unplug, and launch Daybook. Sync runs approximately every 30 seconds.

The setup helper verifies the bot and requires a random pairing phrase from your private chat. The app only accepts messages from that paired chat; group chats and other users are ignored. Tokens are supplied to curl over stdin, not exposed in process arguments. The config remains a plaintext local file on USB storage, which does not enforce reliable per-file access control. Keep physical access and backups private. Runtime files are excluded from Git.

Commands:

```text
/add Buy groceries
/daily Read for 20 minutes
/goal Finish my portfolio
/list
/habits
/progress
/done 3
/undo 3
/edit 3 Read for 30 minutes
/delete 3
/journal Today I made time for reading.
/journal
/jread 1
/jedit 1 Revised thoughts
/jdelete 1
/help
```

Plain text is also added as a to-do. IDs remain stable when other tasks are deleted. `/list` includes current monthly goals; older goals remain stored but leave the current-month view. Daily completion and progress histories are retained locally. Deleting a habit also deletes that habit's per-item grid history; daily aggregate snapshots remain.

Journal commands save and edit dated entries in the same local store as tasks. Sending `/journal` alone lists recent entry IDs; `/jread NUMBER` sends that entry back to your private bot chat. Entry text sent through Telegram is handled by Telegram as well as this app. Entries typed only on the Kindle stay local unless explicitly requested through the bot.

The Mac helper’s `--mac-dialog` option uses a hidden native input box. Tokens are not passed as shell arguments or printed. On other systems, omit it to use the hidden terminal prompt.

Only one device or app may poll the bot. Do not share the bot with another service. Telegram only retains incoming updates for up to 24 hours; this is not an unlimited offline queue. Offline Kindle edits remain local and durable. Live Telegram verification still requires the bot token and on-device Wi-Fi.

Telegram reference: https://core.telegram.org/bots/api#getupdates

## Device verification

On the target KT2, bot pairing and direct HTTPS sync succeeded. A new `/journal` message received a bot reply and appeared in Journal while Daybook was open. Telegram polling runs only while the dashboard is open.
