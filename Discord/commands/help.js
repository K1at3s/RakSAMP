const { SlashCommandBuilder, EmbedBuilder } = require('discord.js');

module.exports = {
    data: new SlashCommandBuilder()
        .setName('help')
        .setDescription('Komut listesini gosterir'),
    async execute(interaction) {
        const embed = new EmbedBuilder()
            .setTitle('🤖 RakSAMP Bot Komutlari')
            .setColor(0x00AEFF)
            .addFields(
                { name: '/link <hwid>', value: 'Sunucuyu RakSAMP HWID\'nize baglar (yonetici)' },
                { name: '/unlink <hwid>', value: 'HWID baglantisini kaldirir (yonetici)' },
                { name: '/setchannel <tur> <kanal>', value: 'Lisans/gstats/war/zonewar/backup/welcome kanalini ayarlar (yonetici)' },
                { name: '/send <komut>', value: 'Oyuna komut/sohbet mesaji iletir (orn. /gstats) (yonetici)' },
                { name: '/gstats', value: 'RakSAMP\'tan istatistik ister' },
                { name: '/zonewar', value: 'Manuel ZONE WAR bildirimi gonderir (yonetici)' },
                { name: '/backup', value: 'Backup cagrisi gonderir (yonetici)' },
                { name: 'Oyun ici ?w', value: 'Oyun icinde ?w yazinca Discord\'a @everyone war yardim cagrisi duser' },
                { name: 'Oyun ici ?1414', value: 'Backup cagrisi → backup kanali (@here)' },
                { name: 'Oyun ici ZONE WAR', value: 'Zone war bildirimleri → zonewar kanali' }
            )
            .setTimestamp();

        await interaction.reply({ embeds: [embed] });
    }
};
