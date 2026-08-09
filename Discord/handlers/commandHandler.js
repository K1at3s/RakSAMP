const fs = require('fs');
const path = require('path');
const { Collection } = require('discord.js');

const commands = new Collection();
const commandsDir = path.join(__dirname, '..', 'commands');

// commands/ klasöründeki tüm komutları yükle
function loadCommands() {
    commands.clear();
    const files = fs.readdirSync(commandsDir).filter(f => f.endsWith('.js'));
    for (const file of files) {
        const cmd = require(path.join(commandsDir, file));
        if (cmd && cmd.data) {
            commands.set(cmd.data.name, cmd);
        }
    }
    console.log(`✅ ${commands.size} komut yüklendi.`);
    return commands;
}

// Slash komutları botun bulunduğu her sunucuya kaydet (anında görünür)
async function registerCommands(client) {
    const data = commands.map(c => c.data.toJSON());
    for (const guild of client.guilds.cache.values()) {
        await guild.commands.set(data);
    }
    console.log(`🆕 ${data.length} slash komut ${client.guilds.cache.size} sunucuya kaydedildi.`);
}

async function registerForGuild(guild, data) {
    await guild.commands.set(data);
}

async function handleInteraction(interaction) {
    if (!interaction.isChatInputCommand()) return;

    const cmd = commands.get(interaction.commandName);
    if (!cmd) return;

    try {
        await cmd.execute(interaction);
    } catch (e) {
        console.error(`Komut hatası (${interaction.commandName}):`, e);
        const msg = { content: '❌ Komut çalıştırılırken hata oluştu.', ephemeral: true };
        if (interaction.replied || interaction.deferred) {
            await interaction.followUp(msg).catch(() => {});
        } else {
            await interaction.reply(msg).catch(() => {});
        }
    }
}

module.exports = { loadCommands, registerCommands, registerForGuild, handleInteraction, commands };
