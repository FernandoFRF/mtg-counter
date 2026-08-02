
# MTG Counter — Contexto do Projeto

> Contador de vida para Magic: The Gathering, rodando numa placa ESP32 com display touch.
> Este arquivo é o contexto principal para o Claude Code.
> Leia antes de qualquer tarefa de desenvolvimento.

---

## Comportamento esperado do Claude Code

* Pode executar as tarefas diretamente, sem esperar aprovação prévia para cada passo
* Antes de cada ação (criar/modificar arquivo, compilar, gravar na placa), explique em detalhe o que está sendo feito e por quê — o raciocínio, não só o resultado
* Objetivo declarado: aprender o processo o suficiente pra manter e evoluir o projeto sozinho depois — priorize explicações que ensinem, não só que resolvam
* Uma tarefa por vez — não antecipe próximas etapas sem ser solicitado
* Faça commits apenas quando solicitado explicitamente

---

## Convenções de Git

### Commits

Seguir o padrão Conventional Commits:

| Prefixo       | Quando usar                                   |
| ------------- | --------------------------------------------- |
| `feat:`     | nova funcionalidade                           |
| `fix:`      | correção de bug                             |
| `docs:`     | documentação                                |
| `refactor:` | mudança de código sem alterar comportamento |
| `chore:`    | configuração, dependências, setup          |

Exemplos:

```
feat: add battery voltage indicator to UI
fix: correct touch coordinate mapping after calibration
chore: add platformio.ini and migrate from Arduino IDE
```

### Branches

* Trabalho novo (feature, fix, experimento) sempre em branch separada, nunca direto na `main`
* Nomenclatura: `feat/nome-da-coisa`, `fix/nome-do-bug` (ex: `feat/battery-mod`, `fix/touch-calibration`)
* Merge para `main` só quando estiver testado na placa física

---

## O que é o MTG Counter

Contador de vida digital para Magic: The Gathering, com tela touch. Fork de [salaroli/mtg_counter_arduino](https://github.com/salaroli/mtg_counter_arduino), mantido em [FernandoFRF/mtg-counter](https://github.com/FernandoFRF/mtg-counter.git).

Feito sob medida pro deck Cycling Storm: acompanha vida do jogador (Cycler) e do oponente, além de contadores específicos de mecânica do deck (Swamp/Storm) e mana disponível por cor.

**Uso:** dispositivo físico dedicado, ligado durante a partida. Sem conexão de rede — WiFi e Bluetooth são desligados no boot para economizar energia.

---

## Stack / Hardware

* **Placa:** ESP32-2432S028R ("CYD" — Cheap Yellow Display), touch **resistivo** (não confundir com a variante capacitiva "C", incompatível)
* **Display:** TFT ILI9341, 2.8", 240×320, controlado via `TFT_eSPI`
* **Touch:** XPT2046 (resistivo), via `XPT2046_Touchscreen` — `lib_deps` aponta pro GitHub oficial (`PaulStoffregen/XPT2046_Touchscreen.git`), não pro registry do PlatformIO: a versão que o registry resolve é antiga e não tem o overload `begin(SPIClass&)` que o código usa
* **Framework:** Arduino, via **PlatformIO** (migrado do Arduino IDE)
* **Armazenamento:** LittleFS (memória flash interna) — **não usa cartão SD**
* **Conectividade:** WiFi e Bluetooth desligados explicitamente no `setup()` (`WiFi.mode(WIFI_OFF)`) para economia de energia

### Pinout

Pinos do **touch** (definidos direto no `.ino`):

```cpp
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
#define TFT_BL 21
```

Pinos do **display TFT** (não estão no `.ino` — vêm dos `build_flags` do
`platformio.ini`, montados à mão a partir da pinagem conhecida da
comunidade pra essa placa; a TFT_eSPI 2.5.43 **não** traz um setup file
pronto pra essa CYD, então não há atalho — os valores abaixo foram
validados empiricamente na placa física):

```ini
build_flags =
    -DUSER_SETUP_LOADED=1
    -DUSE_HSPI_PORT        # obrigatório: sem isso TFT_eSPI e o touch brigam pelo VSPI e o touch para de responder
    -DILI9341_2_DRIVER
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1           # NÃO é um GPIO real nessa placa — usar um pino aqui (ex: 4) deixa as cores lavadas
    -DTFT_BL=21
    -DTFT_INVERSION_ON     # obrigatório com o ILI9341_2_DRIVER nesse lote de painel, senão as cores saem invertidas
    -DSPI_FREQUENCY=40000000
```

Detalhes completos de cada pegadinha (com o sintoma observado e o
diagnóstico) estão em [`docs/GUIA_TECNICO.md`](docs/GUIA_TECNICO.md).

---

## Estrutura de pastas

```
mtg-counter/
├── platformio.ini
├── README.md                # visão geral, build/upload, mapa de zonas de touch
├── docs/
│   └── GUIA_TECNICO.md       # pinagem completa, cada flag explicada, histórico de bugs/pegadinhas
├── src/
│   └── mtg-counter.ino       # sketch principal (renomeado de mtg_counter_arduino.ino)
└── data/
    └── bg.bin                # imagem de fundo, gravada via upload de LittleFS separado do firmware
```

---

## Imagem de fundo (bg.bin)

* Formato: **RGB565 cru** (sem cabeçalho), 240×320, 2 bytes por pixel = 153.600 bytes exatos
* Ordem de bytes: **little-endian** (confirmado empiricamente: decodificando `data/bg.bin` de volta pra imagem e comparando pixel a pixel com a arte-fonte `bg1.png`, little-endian bate quase exato — big-endian dá uma diferença de cor grande. `setSwapBytes(true)` no código lida com a conversão pro que a TFT_eSPI espera internamente, não determina o byte order do arquivo em si)
* Lido do LittleFS no caminho fixo `/bg.bin` — nome não pode mudar sem alterar o código
* Pra gerar um `bg.bin` novo a partir de um PNG 240×320: converter pixel a pixel pra RGB565 e escrever em little-endian (ferramenta de referência usada pelo autor original: https://longfangsong.github.io/en/image-to-rgb565/ — confira o byte order de saída dela antes de usar)
* Upload do `bg.bin` pra placa é **sempre separado** do upload do firmware — comando "Upload Filesystem Image" do PlatformIO

---

## Layout da interface / regras de funcionamento

* **Topo:** vida do Cycler (esquerda) e do Opponent (direita) — toque nas metades esquerda/direita da respectiva zona faz -1/+1
* **Meio:** Swamp (esquerda, ícone caveira) e Storm (direita, ícone nuvem) — contadores específicos do deck; toque e segure no terço superior faz +10
* **Meio-baixo:** 4 contadores de mana (branco, azul, vermelho, verde)
* **Rodapé:** dois botões de long-press — "End of Turn" (zera Swamp/Storm) e "Reset" (retorna todos os valores ao inicial)

---

## Fora do escopo (por enquanto)

* Conectividade WiFi/Bluetooth (desligada intencionalmente, por energia)
* Cartão SD (projeto usa só memória flash interna via LittleFS)
* Bateria integrada — placa alimentada via USB; mod de bateria (TP4056 + LiPo + chave liga/desliga) é modificação física externa, não faz parte do firmware

---

## Convenções de código

* Comentários e nomes de variável seguem o padrão já estabelecido no `.ino` original (majoritariamente em português/inglês misto, conforme o autor original escreveu)
* Mudanças de pinagem sempre via `build_flags` do `platformio.ini`, nunca editando arquivos dentro da pasta da biblioteca instalada (`.pio/libdeps/`) — essas mudanças não sobrevivem a uma reinstalação de dependências

---

## Roadmap futuro (não implementar agora)

<!-- Preencher conforme surgirem ideias — ex: mod de bateria recarregável (TP4056 + LiPo + chave liga/desliga), calibração de touch dedicada, novo tema de bg.bin, etc. -->
