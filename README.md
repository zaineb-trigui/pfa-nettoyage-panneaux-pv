# Conception et création d'un système de nettoyage automatique des panneaux photovoltaïques

Projet Fin d'Année (PFA) - ENET'Com Sfax, Génie des Systèmes Électroniques et de Communication, 2025/2026.

**Auteures :** Zaineb Trigui, Zainab Fritis
**Encadrant :** M. Nidhal Ben Amor
**Soutenance :** 22/04/2026

## Description

Système automatique de nettoyage d'un panneau photovoltaïque : un chariot motorisé parcourt le panneau pendant que l'eau d'une pompe est projetée, afin d'éliminer la poussière et de maintenir le rendement.

## Matériel

- Microcontrôleur ESP32-WROOM-32E
- Moteur DC à encodeur + driver L298N
- Convertisseur DC-DC LM2596
- Horloge temps réel DS3231
- Relais SRD-05VDC (pompe à eau)
- Capteurs : niveau d'eau, pluie, 2 fins de course

## Principe de fonctionnement

1. Calibration au démarrage entre les fins de course bas et haut.
2. Boucle principale : lecture de l'heure, du niveau d'eau, de la pluie et de l'encodeur.
3. Si les conditions sont réunies (heure, eau suffisante, pas de pluie), lancement du nettoyage : 5 cycles aller-retour avec pompe.
4. Sécurités : arrêt immédiat du moteur et de la pompe en cas d'anomalie, watchdog.

## Structure du dépôt

| Dossier | Contenu |
|---|---|
| `docs/` | Rapport PFA (PDF) et présentation (PPTX) |
| `firmware/` | Code embarqué ESP32 (Arduino IDE, C++) |
| `hardware/kicad/` | Schéma électronique KiCad |
| `hardware/solidworks/` | Modèle 3D de la structure mécanique |
| `images/` | Schémas, photos du montage, vues 3D, organigramme |
