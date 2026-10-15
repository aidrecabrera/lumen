# Lumen manual acceptance checklist

## Boot and bring-up
- [ ] device boots without reset loop
- [ ] sensors initialize
- [ ] LED hardware initializes
- [ ] config loads or defaults cleanly
- [ ] Wi-Fi starts
- [ ] MQTT initializes

## Network behavior
- [ ] device connects to Wi-Fi
- [ ] device connects to MQTT broker
- [ ] availability publishes `online`
- [ ] broker loss does not crash node
- [ ] Wi-Fi loss does not crash node
- [ ] reconnect path recovers after broker or Wi-Fi return

## Command behavior
- [ ] valid LED command applies in manual mode
- [ ] LED command is rejected in autonomous mode
- [ ] valid mode command changes mode
- [ ] valid config command updates thresholds
- [ ] invalid command does not corrupt state
- [ ] acknowledgement matches applied or rejected result

## Persistence behavior
- [ ] change mode
- [ ] change LED state
- [ ] change thresholds
- [ ] reboot node
- [ ] values restore correctly

## Short soak run
- [ ] run for at least 2 hours
- [ ] periodic telemetry active
- [ ] at least one mode change during run
- [ ] at least one LED command during run
- [ ] no random reset
- [ ] no dead task symptoms

## Auto mode
- [ ] brightness settles within 2 minutes and does not swing (if it swings, the BH1750 sees the LEDs: move or shield it)
- [ ] lights go off below 10 lux and come back above 20 lux; status shows `night_off`
- [ ] a new lux ceiling from `command/config` survives a reboot

## Setup portal
- [ ] first boot opens `Lumen-Setup-xxyyzz`; the portal saves Wi-Fi details and the local password
- [ ] a mistyped Wi-Fi password opens the portal again
- [ ] holding BOOT for 5 s after a restart opens the portal
- [ ] the portal closes after 10 minutes and the lighting keeps running

## Local dashboard
- [ ] `http://lumen-xxyyzz.local` opens (and the IP address does too)
- [ ] the password prompt protects the page and the WebSocket
- [ ] with the internet unplugged: live readings, LED commands acked
- [ ] two tabs open at the same time, both live; a change in one shows in the other
- [ ] power-cycling the node shows "Node not responding" or "Reconnecting…", then "Live"

## Remote dashboard
- [ ] on mobile data: login required, live readings, LED commands acked
- [ ] a new tab shows the retained status and availability at once
- [ ] power-cycling the node shows "offline", then "online"
- [ ] a tab left open for more than 24 h reconnects by itself
- [ ] a change on the local dashboard shows on the remote dashboard

## Accessibility
- [ ] a screen reader announces the linked share slider changes
- [ ] keyboard-only use reaches every control

## Resources (record in the PR)
- [ ] flash use after build
- [ ] free heap with TLS and the web server both active (stop and review below 40 KB)
