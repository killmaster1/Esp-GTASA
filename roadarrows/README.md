# Road Arrows 0.1.0 — GTA SA 2.10 Android ARM64 / AML

Protótipo de navegação por setas ciano no chão. Marque um destino no mapa, feche o mapa e entre em um veículo. Não precisa apertar botão para ativar. Usa o grafo de ruas do próprio GTA; recalcula a rota conforme você dirige e interrompe as setas ao chegar a 15 metros do destino, remover a marcação ou sair do veículo.

## Baixar e instalar

1. No GitHub, abra **Actions → Build Road Arrows ARM64** e selecione uma execução concluída com sucesso.
2. Em **Artifacts**, baixe **RoadArrows-ARM64** e extraia `libroadarrows.so`.
3. Com o jogo fechado, coloque essa `.so` na pasta de mods usada pelo seu AML. Não substitua `libGTASA.so`. CLEO não é necessário para este mod.
4. Abra o jogo, carregue um save, marque um local próximo numa rua e dirija até ele.

Para desativar, remova somente `libroadarrows.so` com o jogo fechado. Este projeto é independente do HeadCam; a compilação fica em uma ação separada.

## Comportamento e limites desta versão

- Desenha até 16 setas, com espaçamento de 8 metros e alcance de aproximadamente 130 metros à frente. As setas acompanham os segmentos da rede de ruas, com altura ajustada por consultas ao chão.
- Funciona em veículos e na área externa. O cálculo é para ruas; não há navegação aérea, náutica ou por interiores.
- Pode levar alguns segundos para carregar os caminhos de uma região distante. Uma solicitação de carregamento feita por uma missão tem prioridade: o mod não a sobrescreve nem a libera.
- Se não houver caminho válido ou chão carregado, omite as setas naquele trecho. Não inventa uma linha direta atravessando quarteirões.
- Pontes, túneis, bifurcações e destinos fora da rua ainda precisam de teste no aparelho. Para um destino afastado da malha viária, a rota termina na rua encontrada pelo jogo.
- As setas seguem os nós das ruas, sem ajuste fino por faixa. Nós muito espaçados podem produzir curvas menos suaves. Outros mods que alterem a estrutura de caminhos podem exigir adaptação.
- Ao chegar, as setas permanecem desligadas para aquela marcação até ela ser removida ou trocada.

## Compatibilidade e diagnóstico

Biblioteca analisada: GTA SA 2.10 ARM64 enviada pelo usuário, SHA-256 `92b3714534742cf8e31eba35dbfd5e83871ecb5b936f5519d9b6ef3562234f9e`. Dependência mínima declarada: AML 1.4.0. O mod verifica os símbolos e seus endereços relativos antes de instalar o hook, desativando-se caso não correspondam à versão analisada.

Tag de logcat: `RoadArrows`. A mensagem `Road Arrows 0.1.0 enabled` confirma a inicialização; `Vehicle route updated` indica que o jogo retornou uma rota. `Required symbol missing` ou `Unsupported GTA library layout` indica incompatibilidade e nenhuma câmera ou função do jogo será modificada.

Os testes automatizados usam memória e funções simuladas para conferir curvas, carregamento, remoção/troca de marcador, chegada, alturas inválidas e restauração dos estados de desenho, incluindo falha no transformador 3D. Esses testes e a compilação Android não substituem o teste real de execução no celular.

## Compilar

A ação `.github/workflows/roadarrows.yml` executa os testes e compila com NDK 27.2, ABI `arm64-v8a`, Android API 23. Também é possível usar CMake localmente:

```sh
cmake -S roadarrows -B build-roadarrows \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build build-roadarrows --parallel 2
```

Layouts e enums foram conferidos nos dados DWARF da biblioteca ARM64; a busca de rota e o desenho usam as funções nativas. Cabeçalhos AML compartilhados em `../mod/`; licença em `../mod/LICENSE`.
