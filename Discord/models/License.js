const mongoose = require('mongoose');

// HWID bazlı RakSAMP lisans şeması
const LicenseSchema = new mongoose.Schema({
    hwid: { type: String, required: true, unique: true },
    isApproved: { type: Boolean, default: false },
    // Welcome back ayarları (kişiye özel, oyun içi ?greet / ?welcomemsg ile değiştirilir)
    welcomeEnabled: { type: Boolean, default: true },
    welcomeMessage: { type: String, default: '' },
    createdAt: { type: Date, default: Date.now }
});

module.exports = mongoose.model('License', LicenseSchema);
