# MTG Counter — CYD

Contador de vida / storm / mana para Magic: The Gathering, rodando num
ESP32-2432S028R ("CYD" — Cheap Yellow Display), com tela touch resistiva
ILI9341 2.8" 240×320.

## Hardware

- **Placa**: ESP32-2432S028R (CYD)
- **Display**: ILI9341, 240×320, SPI, via [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- **Touch**: XPT2046 resistivo, via [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen)
- **Armazenamento**: LittleFS (imagem de fundo em `/bg.bin`, RGB565 cru)

Detalhes de pinagem, e os porquês de cada flag no `platformio.ini`, estão no
[guia técnico](docs/GUIA_TECNICO.md) — vale ler antes de mexer na
configuração de display/touch.

## Estrutura do projeto

```
platformio.ini       configuração de build (board, libs, pinagem via build_flags)
src/mtg-counter.ino   firmware
data/bg.bin           imagem de fundo (RGB565 cru, 240x320) — gravada via LittleFS
bg1.png, bg.psd       arte-fonte da imagem de fundo (ver "Trocar a imagem de fundo")
docs/GUIA_TECNICO.md  detalhes de hardware, pegadinhas e como debugar
```

## Requisitos

- [PlatformIO Core](https://platformio.org/install/cli) (`pip install platformio`)
- Cabo USB (a placa usa um chip serial CH340)

## Compilar e gravar

```sh
# compilar sem gravar
pio run

# gravar o firmware (ajuste upload_port no platformio.ini pra sua porta serial)
pio run -t upload

# gravar o filesystem LittleFS (a imagem de fundo data/bg.bin)
pio run -t uploadfs

# monitor serial (115200 baud)
pio device monitor
```

`upload_port`/`monitor_port` estão fixados em `platformio.ini` pra facilitar
o dia a dia. Se a placa aparecer noutra porta (outra máquina, outro cabo),
sobrescreva na linha de comando: `pio run -t upload --upload-port COM5`.

O firmware e o filesystem são independentes — só é preciso repetir
`uploadfs` quando `data/bg.bin` mudar.

## Zonas de toque

Tela dividida em 4 faixas verticais (definidas em `Z2_Y`, `Z3_Y`, `Z4_Y` no
código):

| Faixa | Conteúdo | Gesto |
|---|---|---|
| Topo | Vida (Cycler à esquerda, Opponent à direita) | toque: metade externa +1, metade interna −1 |
| Meio | Swamp (esquerda) / Storm (direita) | toque: ±1. Segurar 300ms no terço superior do Swamp: +10 |
| Meio-baixo | 4 manas (W/U/R/G) | toque: linha de cima +1, linha de baixo −1 |
| Rodapé | End of Turn (esquerda) / Reset (direita) | segurar 1s |

Todas as zonas e ações são tabelas declarativas (`Counter[]` e
`TouchRegion[]`) no início do `.ino` — pra mudar um valor, cor ou área de
toque, edita a tabela, não precisa mexer na lógica de dispatch.

## Trocar a imagem de fundo

1. Exporte um PNG de 240×320 (sem alpha, ou com alpha totalmente opaco —
   o firmware não faz composição de transparência).
2. Converta pra RGB565 cru, **little-endian**, sem cabeçalho, com uma
   ferramenta como o [image-to-rgb565](https://longfangsong.github.io/en/image-to-rgb565/).
   (O byte order importa: se sair invertido, o fundo aparece com o mapa de
   cor errado — veja o guia técnico se isso acontecer.)
3. Salve como `data/bg.bin` (deve dar exatamente `240*320*2 = 153600` bytes).
4. `pio run -t uploadfs`.
