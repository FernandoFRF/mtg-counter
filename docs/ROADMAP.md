# Roadmap

Ideias e mods futuros pro MTG Counter. Nada aqui está agendado ou em
implementação — é uma lista de possibilidades pra priorizar quando alguém
tiver tempo/vontade de encarar.

## Prioridade atual

Ordem definida em conversa de planejamento (2026-08-02). Os itens 1 e 3 são
pré-requisito prático do item 5 (case 3D depende de saber o que precisa
caber fisicamente), por isso ficam antes na lista. A lista de compras de
cada mod fica junto do próprio mod, não separada no fim.

### 1. Mod de bateria (TP4056 + LiPo + chave liga/desliga)

**O quê:** carregador TP4056 + bateria LiPo + chave liga/desliga física,
pra usar o contador sem depender de cabo USB na mesa.

**Considerações técnicas:**
- Preferir módulo TP4056 **com proteção** (versão com DW01+FS8205,
  costuma ser vendida como "TP4056 with protection") — evita sobrecarga e
  descarga profunda da LiPo.
- A carcaça original da CYD não tem espaço interno pra bateria — esse mod
  provavelmente exige um case customizado (ver item 5) ou uma solução
  provisória com a bateria montada externamente.
- Checar como a CYD é alimentada (regulador interno, tensão esperada na
  entrada) antes de decidir se a saída do TP4056 pode alimentar a placa
  direto ou se precisa de um conversor step-up.
- A chave liga/desliga deve cortar o circuito fisicamente (não é um
  "sleep" por software) — normalmente fica em série entre a bateria e a
  entrada de alimentação da placa.

**Em aberto:** capacidade da bateria (mAh) — depende de quanto tempo de
uso por partida/torneio se quer e do consumo real do conjunto
display+ESP32 (o backlight do TFT costuma ser o maior consumidor).

**Lista de compras (rascunho, específicações finais em aberto):**
- Módulo TP4056 com proteção (DW01+FS8205)
- Bateria LiPo — capacidade a definir
- Chave liga/desliga (slide switch, em série no circuito de alimentação)
- Conferir compatibilidade de tensão/conector antes de comprar (ver
  "Em aberto" acima)

### 2. Múltiplos layouts de tela

**O quê:** hoje só existe o layout do deck Cycling Storm (vida + swamp/storm
+ manas). Adicionar um modo "simples" — só vida própria e do oponente —
pra partidas genéricas que não usam essas mecânicas.

**Considerações técnicas:**
- Cada layout precisa da própria imagem de fundo e da própria tabela de
  `Counter[]`/`TouchRegion[]` — os arrays já são declarativos no código
  atual, então dá pra ter mais de um conjunto e trocar qual está ativo.
- O LittleFS guardaria mais de uma imagem de fundo (ex.: `/bg_storm.bin`,
  `/bg_simple.bin`).
- Precisa definir como trocar de modo em uso — gesto de toque (ex: toque
  longo num canto neutro) ou botão físico dedicado (ver item 3).
- Se quiser lembrar o último modo usado entre boots, precisa persistir em
  NVS/Preferences — hoje nada é salvo, todo boot reseta pro estado inicial.

**Em aberto:** mecanismo de troca de modo; se a escolha persiste entre
boots.

### 3. Botões físicos

**O quê:** botões dedicados pra funções rápidas, com reset de partida como
exemplo principal. O liga/desliga físico já fica coberto pelo mod de
bateria (item 1) — esse item é sobre botões *de função*, não de energia.

**Considerações técnicas:**
- GPIOs livres na CYD são escassos (a maioria já está em uso por
  TFT/touch/backlight) — precisa mapear quais pinos sobram antes de
  decidir quantos botões cabem.
- Debounce por software — o projeto já tem um padrão de debounce de toque
  no `.ino` (`TAP_DEBOUNCE_MS`), dá pra reaproveitar a mesma ideia.
- Funções além do reset ficam em aberto pra sugerir/detalhar na hora
  (candidata natural: troca de layout do item 2).

**Em aberto:** quantidade de botões, quais GPIOs disponíveis, lista final
de funções.

**Lista de compras (rascunho, quantidade em aberto):**
- Botões táteis (momentâneos) — quantidade a definir junto com a lista de
  funções e os GPIOs disponíveis

### 4. Histórico de modificações de vida

**O quê:** alguma forma de ver as últimas mudanças de vida na tela, sem
depender de anotar no papel (que é o padrão hoje).

**Considerações técnicas:** ainda não definidas — esse é o item com o
requisito menos claro da lista. Formatos possíveis a avaliar quando for
detalhar de verdade:
- Lista simples dos últimos deltas (ex.: "-3, +1, -2...") numa área
  pequena da tela.
- Botão/gesto de "desfazer última ação" — resolve o caso de uso mais
  comum (toque errado) sem precisar de UI de histórico.
- Log completo com timestamp — mais completo, porém mais complexo e
  consome mais RAM/espaço de tela.

**Em aberto:** praticamente tudo — formato de exibição, quanto histórico
guardar, se cabe na tela sem atrapalhar o resto do layout.

### 5. Case 3D + atualizar README de hardware

**O quê:** dois entregáveis relacionados, que só fazem sentido depois do
hardware (bateria + botões) estar fechado:
- Modelagem de um case 3D que comporte os mods de bateria e botões
  físicos.
- Seção de hardware completa no README (ou um `docs/HARDWARE.md`
  dedicado), pra quem quiser replicar o projeto do zero.

**Dependência:** faz mais sentido detalhar esse item **depois** dos itens
1 e 3, já que o design do case e a documentação final dependem de saber
que componentes/tamanhos precisam caber fisicamente.

## Outras ideias (sem prioridade definida)

* **Atualização de firmware via WiFi (OTA)**, em vez de precisar plugar USB
  toda vez. Traz de volta a necessidade de WiFi (hoje desligado por
  economia de energia — reavaliar o trade-off) e exige guardar credencial
  de rede em algo tipo `secrets.h`, que **precisa** entrar no `.gitignore`
  desde o commit inicial da feature (senha commitada uma vez fica no
  histórico do git para sempre, mesmo se o arquivo for apagado depois)
* Calibração de touch dedicada (rotina de setup guiada, em vez dos valores
  fixos hardcoded)
* Novo tema de `bg.bin` (relacionado ao item 2 — múltiplos layouts também
  podem significar múltiplos temas visuais, não só telas de conteúdo
  diferente)
