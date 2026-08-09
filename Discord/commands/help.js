const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('help')
        .setDescription('Komut listesini gösterir'),
    async execute(interaction) {
        const embed = new EmbedBuilder()
            .setTitle('🤖 RakSAMP Bot Komutları')
            .setColor(0x00AEFF)
            .addFields(
                { name: '/link <hwid>', value: 'Sunucuyu RakSAMP HWID\'nize bağlar (yönetici)' },
                { name: '/unlink <hwid>', value: 'HWID bağlantısını kaldırır (yönetici)' },
                { name: '/setchannel <tür> <kanal>', value: 'Lisans/gstats/war/zonewar/backup/welcome kanalını ayarlar (yönetici)' },
                { name: '/send <komut>', value: 'Oyuna komut/sohbet mesajı iletir (örn. /gstats) (yönetici)' },
                { name: '/gstats', value: 'RakSAMP\'tan istatistik ister' },
                { name: '/zonewar', value: 'Manuel ZONE WAR bildirimi gönderir (yönetici)' },
                { name: '/backup', value: 'Backup çağrısı gönderir (yönetici)' },
                { name: 'Oyun içi ?w', value: 'Oyun içinde ?w yazınca Discord\'a @everyone war yardım çağrısı düşer' },
                { name: 'Oyun içi ?1414', value: 'Backup çağrısı → backup kanalı (@here)' },
                { name: 'Oyun içi ZONE WAR', value: 'Zone war bildirimleri → zonewar kanalı' }
            )
            .setTimestamp();

        await interaction.reply({ embeds: [embed] });
    }
};
