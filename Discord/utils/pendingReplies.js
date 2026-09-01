// interactionId -> interaction eslesmesi.
// /gstats, /stats, /astats komutlari oyundan yanit gelince buradan interaction'i bulup
// editReply ile sonucu komutu calistiran kisiye gondeririz.
const pendingInteractions = new Map();

function setPendingInteraction(id, interaction) {
    if (!id || !interaction) return;
    pendingInteractions.set(id, interaction);
    // 10 dakika sonra otomatik temizle (bellek guvenligi)
    setTimeout(() => {
        if (pendingInteractions.get(id) === interaction) pendingInteractions.delete(id);
    }, 10 * 60 * 1000);
}

function getPendingInteraction(id) {
    return pendingInteractions.get(id) || null;
}

function removePendingInteraction(id) {
    pendingInteractions.delete(id);
}

module.exports = { setPendingInteraction, getPendingInteraction, removePendingInteraction };
