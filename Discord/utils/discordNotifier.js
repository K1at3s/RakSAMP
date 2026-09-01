// Discord tarafindan (slash komutlar) ve REST endpoint'lerden kullanilan
// ortak "kanala mesaj gonder" yardimcisi. client, server.js icinde set edilir.
let _client = null;

function setClient(client) {
    _client = client;
}

// guild + kanal cozumle, Discord'a embed/duz mesaj/dosya gonder.
// Kanal ayarlanmamissa null doner (istek basarili sayilir, sadece loglanir).
async function sendToGuildChannel(guild, field, { embed, content, files } = {}) {
    const channelId = guild ? guild[field] : null;
    if (!channelId) {
        console.log(`⚠️ ${field} kanali ayarlanmamis (guild=${guild ? guild.guildId : '?'}), mesaj atlaniyor.`);
        return null;
    }
    try {
        const channel = await _client.channels.fetch(channelId);
        if (!channel) {
            console.log(`⚠️ ${field} kanali bulunamadi (id=${channelId}).`);
            return null;
        }
        if (embed) await channel.send({ embeds: [embed], files });
        else if (files) await channel.send({ files });
        else if (content) await channel.send(content);
        return channelId;
    } catch (e) {
        console.error(`Discord gonderim hatasi (${field}):`, e);
        return null;
    }
}

module.exports = { setClient, sendToGuildChannel };
