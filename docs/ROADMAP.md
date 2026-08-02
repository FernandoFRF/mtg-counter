# Roadmap

Ideias e mods futuros pro MTG Counter. Nada aqui está agendado ou em
implementação — é uma lista de possibilidades pra priorizar quando alguém
tiver tempo/vontade de encarar.

## Ideias

* **Atualização de firmware via WiFi (OTA)**, em vez de precisar plugar USB
  toda vez. Traz de volta a necessidade de WiFi (hoje desligado por
  economia de energia — reavaliar o trade-off) e exige guardar credencial
  de rede em algo tipo `secrets.h`, que **precisa** entrar no `.gitignore`
  desde o commit inicial da feature (senha commitada uma vez fica no
  histórico do git para sempre, mesmo se o arquivo for apagado depois)
* Mod de bateria recarregável (TP4056 + LiPo + chave liga/desliga)
* Calibração de touch dedicada (rotina de setup guiada, em vez dos valores
  fixos hardcoded)
* Novo tema de `bg.bin`
