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
