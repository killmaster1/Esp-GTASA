# Head Camera 0.1.0 — GTA SA 2.10 ARM64 / AML 1.4

Protótipo independente de primeira pessoa. Inspirado no comportamento solicitado,
não é um port do código de First Person Mod v3.0 Fixed e não promete os mesmos recursos.

## Comportamento

- Ativa automaticamente quando instalado.
- Posiciona a câmera na cabeça animada de CJ ao andar e nos modos nativos de seguir veículos.
- Mantém a direção de visão e os controles de olhar do jogo.
- Ao entrar e sair do veículo, continua tentando acompanhar a cabeça enquanto
  o jogo usar os modos de acompanhamento permitidos.
- Oculta o modelo inteiro de CJ durante a primeira pessoa para evitar enxergar
  o interior da cabeça. NPCs continuam visíveis.
- Usa plano de corte próximo de 0,06 para permitir visualizar a cabine.
- Cenas, widescreen, morte e modos especiais de câmera/mira mantêm a câmera nativa.
- Não implementa braços/armas em primeira pessoa, inclinação conforme a orientação
  do osso, menu, botão de alternância ou configurações por modelo de veículo.

## Compilar no GitHub

Actions → Build First Person ARM64 → Run workflow. O workflow também inicia
quando os arquivos de firstperson, seus cabeçalhos AML ou o próprio workflow mudam.
Baixe o artefato `HeadCam-ARM64` e extraia `libheadcam.so`.

## Instalar

Com o GTA fechado, coloque `libheadcam.so` na pasta de mods do AML onde está
`libnpclines.so`. São mods separados; este não precisa do ESP para funcionar.
Para o primeiro teste, use apenas HeadCam entre os mods opcionais para facilitar
identificar problemas. Abra um save, ande e entre num carro padrão.
Para desativar, feche o jogo e remova apenas `libheadcam.so`.

Se fechar ou ficar com a câmera incorreta, envie o crashlog e descreva a ação
(ex.: andando, entrando no carro, dirigindo ou saindo).
Logs de inicialização usam a tag `HeadCam`.

## Validação e limites

Biblioteca alvo, SHA-256:
`92b3714534742cf8e31eba35dbfd5e83871ecb5b936f5519d9b6ef3562234f9e`.

Offsets de CCam, CCamera, CPed, CPlaceable e RwCamera conferidos nos dados DWARF
fornecidos. Os endereços relativos dos símbolos também são verificados ao carregar;
outros builds são rejeitados. Isso não valida patches feitos por outros mods.

O workflow compila com Android NDK. Testes no host usam memória e funções simuladas
para verificar aplicação/restauração da posição e do near clip, exclusão de modos
especiais, ocultação somente de CJ e rejeição de ossos inválidos.
Esses testes não executam o motor do GTA e não validam ABI ou renderização no aparelho.

Ainda não foi testado em um Android real: animações de entrada/saída, colisões com
paredes, veículos modificados, reflexos, retrovisores e interação com missões precisam
ser avaliados. Modelos sem cabine detalhada podem mostrar partes ausentes por dentro.
O plano próximo reduz a precisão de profundidade e pode produzir artefatos à distância.

## Compilar com NDK local

```sh
cmake -S firstperson -B build-firstperson \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build build-firstperson --parallel 2
```

Cabeçalhos AML e respectiva licença estão em `../mod/`.
