# Guia técnico

Este documento registra o *porquê* de cada decisão não óbvia no
`platformio.ini` e no firmware — principalmente as pegadinhas descobertas
na migração do Arduino IDE pro PlatformIO, pra não precisar redescobrir
tudo de novo da próxima vez que algo quebrar.

## Pinagem

### Display (TFT_eSPI, via `build_flags`)

| Sinal | Pino | Observação |
|---|---|---|
| MISO | 12 | |
| MOSI | 13 | |
| SCLK | 14 | |
| CS   | 15 | |
| DC   | 2  | |
| RST  | **-1** | **Não é um GPIO real.** Nas placas CYD o reset do painel não é controlado por software — se você definir um pino aqui (ex: `4`), a `tft.init()` manda os comandos de configuração sem um reset de hardware limpo, e o painel fica com cores erradas/lavadas (foi o primeiro bug que apareceu nesta migração). |
| BL   | 21 | Backlight, controlado também via `pinMode`/`digitalWrite` no `setup()` |

### Touch (XPT2046, definido no próprio `.ino`)

| Sinal | Pino |
|---|---|
| IRQ  | 36 |
| MOSI | 32 |
| MISO | 39 |
| CLK  | 25 |
| CS   | 33 |

## `platformio.ini` — flags explicadas

```ini
-DUSER_SETUP_LOADED=1     # diz pra TFT_eSPI ignorar o User_Setup.h da lib e usar só as flags abaixo
-DUSE_HSPI_PORT           # ver "Conflito de barramento SPI" abaixo — crítico, sem isso o touch não funciona
-DILI9341_2_DRIVER        # sequência de init "alternativa" do ILI9341 (não a padrão) — ver "Cores erradas" abaixo
-DTFT_INVERSION_ON        # necessário junto com o driver acima nesse lote de painel — ver "Cores erradas"
-DSPI_FREQUENCY=40000000       # clock do barramento do display
-DSPI_TOUCH_FREQUENCY=2500000  # clock do barramento do touch (só usado se TFT_eSPI tivesse touch próprio — aqui é vestigial, mantido por padrão)
-DLOAD_FONT2 / LOAD_GFXFF / SMOOTH_FONT  # o sketch usa setTextFont(2) pros números de mana e setFreeFont() (FreeSans18pt7b etc) pra vida/swamp/storm — sem essas flags a lib nem compila esses símbolos
```

## Pegadinhas encontradas (e como resolvemos)

### 1. Lib do touch desatualizada no registry do PlatformIO

`lib_deps = paulstoffregen/XPT2046_Touchscreen` resolve pra uma versão
antiga (`0.0.0-alpha`) que só tem `begin()` sem argumento — não tem o
overload `begin(SPIClass&)` que o firmware usa pra rodar o touch num
barramento SPI customizado. **Solução**: apontar direto pro GitHub:

```ini
lib_deps =
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
```

### 2. Conflito de barramento SPI entre display e touch (touch morto)

Sintoma: `ts.touched()` sempre `true`, com `getPoint()` retornando sempre
`x=4095 y=0 z=4095` (valores de "linha flutuando em nível alto") —
independente de estar tocando a tela ou não.

Causa: por padrão, no ESP32, a `TFT_eSPI` usa o periférico de hardware
**VSPI** (`Processors/TFT_eSPI_ESP32.c`, a menos que `USE_HSPI_PORT` esteja
definido). O firmware também cria seu próprio `SPIClass(VSPI)` pro touch,
nos pinos 25/32/39/33. Como `tft.init()` roda *depois* do `ts.begin()` no
`setup()`, ele reconfigura o barramento VSPI pros pinos do display
(12/13/14/15), atropelando a configuração que o touch tinha acabado de
fazer.

**Solução**: `-DUSE_HSPI_PORT`, que faz a TFT_eSPI usar o periférico HSPI
em vez de VSPI, deixando o VSPI exclusivo pro touch.

Se o touch voltar a parar de funcionar depois de alguma mudança de lib/config,
esse é o primeiro lugar a olhar — reative o debug temporário descrito em
"Debugando o touch" abaixo pra confirmar rapidamente.

### 3. Cores lavadas / invertidas

Testamos várias combinações de driver + inversão nesse painel específico:

| Driver | `TFT_INVERSION_ON` | Resultado |
|---|---|---|
| `ILI9341_DRIVER` (padrão) | não | tudo errado — fundo branco, cores trocadas (ex: sol aparecia azul) |
| `ILI9341_2_DRIVER` | não | lavado/branco |
| `ILI9341_2_DRIVER` | **sim** | **correto** ✅ |

`ILI9341_2_DRIVER` é uma sequência de init "alternativa" da TFT_eSPI pra
esse tipo de painel ([issue original](https://github.com/Bodmer/TFT_eSPI/issues/1172)),
mas ela não manda nenhum comando de inversão de cor por conta própria —
sem a flag `TFT_INVERSION_ON`, o painel fica no estado de fábrica dele, que
pra esse lote é o "errado".

Tentamos também trocar a tabela de gamma (via `tft.writecommand`/`writedata`
depois do `tft.init()`, usando a tabela clássica do `ILI9341_DRIVER` só
pra gamma) pra tentar corrigir uma leve dessaturação nos ícones de mana —
**não fez diferença nenhuma**, o que indica que gamma não era a causa.
Não persistimos essa mudança no código.

A dessaturação residual nos ícones (perceptível a olho nu, mas some quase
totalmente em foto — a câmera capta as cores reais que o painel está
emitindo, o olho vendo direto percebe menos) foi atribuída à natureza do
painel TN barato dessas placas (baixo contraste fora do eixo de visão) e/ou
a alguma película protetora de fábrica ainda colada no vidro. Confirmamos
que **os dados gravados em `data/bg.bin` estão corretos** (decodificados de
volta batem quase exatamente com a arte-fonte `bg1.png`), então não há mais
nada pra investigar do lado do firmware — se voltar a incomodar, comece
verificando a película física antes de mexer em configuração.

### 4. Ambiente Windows: PlatformIO CLI

Se o Python usado for o da Microsoft Store, ele vem com um `pip.ini`
interno que força `install.user = yes` em toda instalação. Isso conflita
com o instalador interno do PlatformIO (usa `pip install --target ...` pra
empacotar ferramentas como o `esptoolpy`) e quebra com
`ERROR: Can not combine '--user' and '--target'`.

**Contorno**: definir `PIP_USER=0` no ambiente antes de rodar comandos do
`pio` (tem precedência sobre o `pip.ini` de escopo "site"):

```sh
export PIP_USER=0   # bash
$env:PIP_USER = "0" # PowerShell
pio run
```

(Solução definitiva seria instalar um Python "de verdade", não o da
Microsoft Store, mas o contorno acima resolve sem precisar mexer no
sistema.)

Além disso, `pip install platformio` não necessariamente coloca `pio.exe`
no `PATH`. Se `pio` não for reconhecido, use `python -m platformio` no
lugar de `pio` em qualquer comando deste guia.

## Debugando o touch

Se o touch parar de responder, o jeito mais rápido de diagnosticar é
imprimir os valores brutos no serial. Adicione temporariamente no início
do `loop()`, logo depois de `TS_Point p = ts.getPoint();`:

```cpp
Serial.printf("raw x=%d y=%d z=%d\n", p.x, p.y, p.z);
```

e observe com `pio device monitor`. Valores travados em `x=4095 y=0 z=4095`
(ou similar, sempre iguais) indicam problema de comunicação SPI (ver
pegadinha #2 acima), não um problema de calibração — nesse caso os valores
variam conforme onde você toca, mesmo que a posição mapeada na tela esteja
errada.

Lembre de remover o print antes de comitar — ele não deve ficar no
firmware final (polui o serial e custa alguns ciclos por loop).

## Formato de `data/bg.bin`

RGB565 cru, **little-endian**, sem cabeçalho, 240×320 pixels
(`240*320*2 = 153600` bytes), lido linha a linha pelo firmware
(`drawBackground()`/`patchBackground()` em `src/mtg-counter.ino`).

Confirmado empiricamente decodificando `data/bg.bin` de volta pra imagem e
comparando pixel a pixel com a arte-fonte (`bg1.png`) — bateu quase exato
(diferença média por canal ~7, dentro do erro de arredondamento do
RGB565). Se gerar um novo `bg.bin` e as cores saírem trocadas/estranhas, o
mais provável é a ferramenta de conversão ter usado byte order ou ordem de
canal diferente — refaça esse teste de decodificação antes de gastar tempo
mexendo em `TFT_INVERSION_ON`/driver, já que aqueles resolvem um problema
diferente (polaridade do painel, não formato do arquivo).
