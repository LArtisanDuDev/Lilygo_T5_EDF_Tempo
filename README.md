# Affichage des couleurs des jours EDF TEMPO sur Lilygo T5 (ESP32)

## 📝 Description Générale

Ce dépôt contient le code source pour un dispositif qui affiche les informations relatives au tarif TEMPO d'EDF sur un écran E-Ink, en utilisant un microcontrôleur ESP32. Le dispositif récupère les données TEMPO en temps réel via une API et affiche la couleur du jour et du lendemain.<br>
Ce projet était initialement basé sur https://github.com/kaloskagatos/EDF-Tempo-E-Ink-Display.<br>
Je ne maintiens plus la branche pour Arduino IDE aussi j'ai supprimé mon fork et recréé ce projet.<br>
Nouveauté 2024 : les API EDF ne répondent plus. Migration du projet vers les API RTE (https://data.rte-france.com/)
<br>
Photo du projet avant la mise à jour 2024
![eTempo Display](doc/eTempo.jpg)

## 🌐 Connexion WiFi

Pour la configuration du réseau WiFi vous devez renseigner les variables wifi_ssid et wifi_key dans le fichier TOCUSTOMIZE.h avec vos informations.

## 🌐 API RTE

Pour utiliser les API RTE vous devez vous créer un compte ici : https://data.rte-france.com/create_account<br>
Ensuite, il faut vous abonner à l'api : https://data.rte-france.com/catalog/-/api/consumption/Tempo-Like-Supply-Contract/v1.1<br>
Puis créer une application de type MOBILE<br>
Vous aurez alors accès à vos client id et client secrets qu'il faudra renseigner dans le fichier TOCUSTOMIZE.h

## ⏰ Heures de Réveil Préprogrammées

Le dispositif est programmé pour se réveiller à 2h00, 6h30 et 11h05

## 🖥️ Matériel Utilisé

- **Board ESP-32 E-Ink**: T5 V2.3.1 - Écran E-Paper 2.13 pouces à faible consommation d'énergie, modèle GDEM0213B74 CH9102F [Q300]
  - [Lien vers le produit](https://www.lilygo.cc/products/t5-v2-3-1)

## Librairies externes utilisées 

* https://github.com/bblanchon/ArduinoJson
* https://github.com/ZinggJM/GxEPD
* https://github.com/LArtisanDuDev/MyDumbWifi
* https://github.com/LArtisanDuDev/TempoLikeSupplyContractAPI

## 📄 Licence

Ce projet est distribué sous la licence GNU General Public License v3.0. Pour plus de détails, veuillez consulter le fichier `LICENSE` dans ce dépôt.

---

## Contribution

Les contributions à ce projet sont les bienvenues. Si vous souhaitez contribuer, veuillez suivre les directives de contribution standards pour les projets GitHub.

## Support et Contact

Pour le support ou pour entrer en contact, veuillez ouvrir un ticket dans la section issues du dépôt GitHub.

## Bugs connus

* Le preview RTE n'a pas encore pu être réellement testée
* Pas de gestion d'erreur, pas de retry
* Couleurs en anglais
