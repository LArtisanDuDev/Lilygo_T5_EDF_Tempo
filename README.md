# Affichage des couleurs des jours EDF TEMPO sur Lilygo T5 (ESP32)

## 📝 Description Générale

Ce dépôt contient le code source pour un dispositif qui affiche les informations relatives au tarif TEMPO d'EDF sur un écran E-Ink, en utilisant un microcontrôleur ESP32. Le dispositif récupère les données TEMPO en temps réel via une API et affiche la couleur du jour et du lendemain
Ce projet est basé sur le projet suivant https://github.com/kaloskagatos/EDF-Tempo-E-Ink-Display adapté pour platformio.
Nouveauté 2024 : les API EDF ne répondent plus. Quick Fix : l'api maintenant utilisée est la preview RTE en attendant de trouver mieux.

Photo du projet avant la mise à jour 2024
![eTempo Display](doc/eTempo.jpg)

## 🌐 Connexion WiFi

Pour la configuration du réseau WiFi vous devez renseigner les variables wifi_ssid et wifi_key dans le fichier TOCUSTOMIZE.h avec vos informations.

## ⏰ Heures de Réveil Préprogrammées

Le dispositif est programmé pour se réveiller toutes les heures

## 🖥️ Matériel Utilisé

- **Board ESP-32 E-Ink**: T5 V2.3.1 - Écran E-Paper 2.13 pouces à faible consommation d'énergie, modèle GDEM0213B74 CH9102F [Q300]
  - [Lien vers le produit](https://www.lilygo.cc/products/t5-v2-3-1)

## Pré-requis 

* https://github.com/ZinggJM/GxEPD
* https://github.com/LArtisanDuDev/MyDumbWifi

## 📄 Licence

Ce projet est distribué sous la licence GNU General Public License v3.0. Pour plus de détails, veuillez consulter le fichier `LICENSE` dans ce dépôt.

---

## Contribution

Les contributions à ce projet sont les bienvenues. Si vous souhaitez contribuer, veuillez suivre les directives de contribution standards pour les projets GitHub.

## Support et Contact

Pour le support ou pour entrer en contact, veuillez ouvrir un ticket dans la section issues du dépôt GitHub.
