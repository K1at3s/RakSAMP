// Discord tarafından (slash komutlar) ve REST endpoint'lerden kullanılan
// ortak "kanala mesaj gönder" yardımcısı. client, server.js içinde set edilir.
let _client = null;

function setClient(client) {
    _client = client;
}

// guild + kanal çözümle, Discord'a embed/düz mesaj/dosya gönder.
// Kanal ayarlanmamışsa null döner (istek başarılı sayılır, sadece loglanır).
async function sendToGuildChannel(guild, field, { embed, content, files } = {}) {
    const channelId = guild ? guild[field] : null;
    if (!channelId) {
        console.log(`⚠️ ${field} kanalı ayarlanmamış (guild=${guild ? guild.guildId : '?'}), mesaj atlanıyor.`);
        return null;
    }
    try {
        const channel = await _client.channels.fetch(channelId);
        if (!channel) {
            console.log(`⚠️ ${field} kanalı bulunamadı (id=${channelId}).`);
            return null;
        }
        if (embed) await channel.send({ embeds: [embed], files });
        else if (files) await channel.send({ files });
        else if (content) await channel.send(content);
        return channelId;
    } catch (e) {
        console.error(`Discord gönderim hatası (${field}):`, e);
        return null;
    }
}

module.exports = { setClient, sendToGuildChannel };
