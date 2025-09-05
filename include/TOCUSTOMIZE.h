#include <Arduino.h>
// ==================================
//           CUSTOMIZE BEGIN
// ==================================

bool tempoSansCompteTRE = true;

// Create an account and your app here : https://data.rte-france.com/create_account/
String client_secret = "";
String client_id = "";

const char* wifi_ssid = "";
const char* wifi_key = "";

// Utilisé pour compter les jours sur le service avec inscription
String debutSaisonTempo = "2025-09-01";

// Pour les apis sans inscription
String saisonTempo = "2025-2026";

// ==================================
//           CUSTOMIZE END
// ==================================
