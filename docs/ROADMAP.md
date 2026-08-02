# Roadmap

Ideias e mods futuros pro MTG Counter. Nada aqui está agendado ou em
implementação — é uma lista de possibilidades pra priorizar quando alguém
tiver tempo/vontade de encarar.

## Prioridade atual

Ordem definida em conversa de planejamento (2026-08-02). Os itens 1 e 3 são
pré-requisito prático do item 5 (case 3D depende de saber o que precisa
caber fisicamente), por isso ficam antes na lista. A lista de compras de
cada mod fica junto do próprio mod, não separada no fim.

### 1. Mod de bateria (módulo carrega+boost + LiPo + chave liga/desliga)

**O quê:** módulo de bateria + bateria LiPo + chave liga/desliga física,
pra usar o contador sem depender de cabo USB na mesa.

**Considerações técnicas (pesquisado em 2026-08-02):**
- Confirmado em duas fontes técnicas independentes: a CYD **não tem
  conector nem circuito de bateria de fábrica** — alimentação é só via
  micro-USB 5V, com dois reguladores AMS1117@3.3V internos. Ou seja, um
  TP4056 puro **não é suficiente sozinho**: ele carrega a bateria mas só
  entrega 3.7–4.2V, e a placa precisa de 5V — falta um conversor step-up
  no meio.
- Consumo medido pela comunidade: **~115mA com o backlight no brilho
  máximo** (cai ~40% escurecendo via PWM no pino do backlight).
- Decisão tomada: usar um módulo **tudo-em-um** (carrega + boost 5V, tipo
  "UPS" com power-path de verdade) em vez de TP4056 + step-up separados —
  menos fiação e mais seguro pra um primeiro mod. A alternativa com
  módulos separados (TP4056 + MT3608) é mais barata mas exige cuidado na
  fiação porque um TP4056 puro não faz power-path (carregar e usar ao
  mesmo tempo com segurança).
- A maioria dos módulos desse tipo é vendida pra bateria **18650**
  (cilíndrica, com suporte metálico), mas o circuito interno é o mesmo de
  uma LiPo de bolso — quase todos trazem também um pad JST-PH pra soldar
  a bateria direto, sem usar o suporte cilíndrico. **Conferir isso na
  descrição do produto antes de comprar**, já que o suporte cilíndrico não
  cabe bem num case fino.
- A chave liga/desliga deve ficar na **saída de 5V** do módulo (entre o
  módulo e a CYD), não na linha da bateria — assim o carregamento
  continua funcionando com o aparelho desligado (comportamento de UPS de
  verdade: pluga o USB, carrega, independente da chave).
- A carcaça original da CYD não tem espaço interno pra bateria — esse mod
  depende do case customizado do item 5.

**Em aberto:** capacidade final da bateria — estimativa grosseira de
1500–2000mAh dá ~6–9h de uso contínuo (considerando perda do boost), mas
relatos da comunidade com setups parecidos variam bastante (1h30 a 5h) —
vale medir na prática depois de montado.

**Pra revisar na próxima sessão:** thread com discussão específica sobre
alimentar a CYD por bateria —
[issue #47](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/issues/47),
em especial [este comentário](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/issues/47#issuecomment-2311551210)
(ainda não lido/avaliado).

**Lista de compras:**
- Módulo carregador + boost 5V (tudo-em-um): [DD05CVSA — Usinainfo](https://www.usinainfo.com.br/carregador-de-bateria/modulo-carregador-e-boost-para-bateria-litio-18650-com-saida-5v-12a-dd05cvsa-9137.html)
  ou alternativa no Mercado Livre: [Módulo Step Up UPS Carregador, entra 5V sai 5V](https://www.mercadolivre.com.br/modulo-step-up-ups-carregador-1s-18650-entra-5v-e-sai-5v-nfe/up/MLBU603703887)
- Bateria LiPo, conector JST-PH 2.0mm 2 pinos, 1500–2000mAh: [listagem no Mercado Livre](https://lista.mercadolivre.com.br/bateria-3.7v-conector-jst)
- Chave liga/desliga (slide switch): [pack de 5un, 0.3A/50V DC — Mercado Livre](https://produto.mercadolivre.com.br/MLB-3170465443-mini-chave-liga-desliga-pacote-com-5-unidades-_JM)
  (bem acima do consumo da placa, ~115–180mA)

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

**Referências:**
- [ESP32 Cheap Yellow Display — USB-C version enclosure (Printables)](https://www.printables.com/model/744864-esp32-cheap-yellow-display-usb-c-version-enclosure)
  — ponto de partida pra avaliar depois; conferir se é compatível com a
  revisão da placa usada aqui (o link é da variante USB-C, nossa placa é
  micro-USB) antes de adotar como base.

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
