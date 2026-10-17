const Guild = require('../models').Guild;

function language(guild, interaction) {
    if (guild?.lang === 'tr' || guild?.lang === 'en') return guild.lang;
    return String(interaction?.locale || interaction?.guildLocale || 'en').startsWith('tr') ? 'tr' : 'en';
}

function translator(guild, interaction) {
    const lang = language(guild, interaction);
    return (tr, en) => lang === 'tr' ? tr : en;
}

async function interactionTranslator(interaction) {
    if (interaction.botTranslator) return interaction.botTranslator;
    const guild = await Guild.findOne({ guildId: interaction.guildId });
    interaction.botTranslator = translator(guild, interaction);
    return interaction.botTranslator;
}

module.exports = { language, translator, interactionTranslator };
