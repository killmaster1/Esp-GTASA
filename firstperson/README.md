# Head Camera 0.2.1 — GTA SA 2.10 ARM64 / AML 1.4

Implementação própria inspirada no First Person Mod v3.0 Fixed.

## Correções em 0.2.1

Os vídeos do teste mostraram câmera invertendo ao movimentar CJ e retornando à terceira
pessoa ao atirar. Esta versão mantém intactas as colunas de orientação calculadas pelo
jogo e desloca somente a posição. A base lógica usada para os olhos e ossos vem dos
vetores Front/Up de CCam, sem reconstruir a matriz interna de CCamera.

A inclinação animada adicional foi retirada. Os modos de mira comum 5, 53 e 55 mantêm
a primeira pessoa mesmo quando o alvo armazenado é outro personagem. Mira com luneta,
lança-foguetes e câmeras de roteiro continuam nativas. Não há novos modelos/animações
de armas, e a mira/colisão dos disparos não foi recalibrada.

A ausência de dados opcionais de renderização do esqueleto já não desativa a câmera:
nesse caso só o alinhamento dos ossos é ignorado.

Os testes incluem matrizes internas com convenções de eixos diferentes da base lógica,
comparação byte a byte da orientação e transições entre acompanhamento e mira.

## Corpo e posição dos olhos

- O desenho de CJ não é mais bloqueado: torso, braços e pernas usam a renderização normal.
- A cabeça e os ossos faciais padrão são alinhados temporariamente ao olhar durante
  o desenho. A transformação preserva a escala e a distância entre os ossos.
- Os dados dos ossos são restaurados byte a byte após o desenho. Reflexos mantêm o
  caminho nativo quando a flag de espelho do jogo está ativa.
- O ponto de visão usa o deslocamento reconstruído da configuração original de CJ:
  0,11962963 para frente e 0,09370371 para cima, na base local da câmera.
- O plano de corte próximo passa a 0,053333335, conforme o cfg analisado.
- A orientação nativa da câmera permanece intacta; não é aplicada inclinação adicional.
- Continua usando a direção de olhar dos controles nativos de toque.

A análise do mod de PC mostrou alinhamento da cabeça e deslocamento dos olhos;
não confirmou que ele apague a cabeça. Esta implementação adapta essa ideia ao
Android manipulando temporariamente matrizes de skin no desenho. Não copia a
rotina binária de PC nem garante reprodução idêntica de todas as suas animações.

## Comportamento

Ativa automaticamente quando instalada. Segue a cabeça ao andar e nos modos nativos
de acompanhar veículos, incluindo as animações de entrada/saída enquanto esses modos
permanecem ativos. Cenas, widescreen, morte e miras especiais com luneta mantêm a câmera
do jogo; mira comum mantém primeira pessoa. FOV permanece nativo.

Não inclui braços/armas com animações novas, rotação independente completa do corpo,
controle de olhar próprio para veículos, offsets por modelo, menu ou botão de alternância.
As mãos que aparecerem são as animações normais do personagem. O enquadramento depende
do modelo do veículo e da animação. Não foram aplicadas sem validação as 40 entradas
de veículos do cfg de PC.

## Baixar e instalar

Actions → Build First Person ARM64 → abra a execução mais recente com sucesso →
Artifacts → HeadCam-ARM64. Extraia `libheadcam.so` do ZIP.

Feche o GTA e substitua a versão anterior de `libheadcam.so` na pasta de mods do AML.
Não mantenha a antiga renomeada com extensão `.so`, pois o AML pode carregar ambas.
O ESP `libnpclines.so` é um mod separado. Para o primeiro teste, use somente HeadCam
entre os mods opcionais para facilitar identificar problemas.

Abra um save, ande, olhe para baixo, entre num carro padrão, dirija e saia.
Para desativar, feche o jogo e remova `libheadcam.so`.

## Validação

- Símbolos e estruturas conferidos na libGTASA.so ARM64 fornecida, incluindo
  RpHAnimHierarchy (matrizes em +8), RwMatrixTag (64 bytes) e CCam/CCamera.
- Testes com memória/funções simuladas verificam câmera e near clip, restauração dos
  eixos nativos entre quadros, preservação da orientação dentro de cada quadro,
  transições de mira comum, desenho do CJ, rotação rígida dos ossos faciais,
  preservação do restante do corpo, restauração byte a byte, espelhos e dados inválidos.
- Testes locais também passaram com AddressSanitizer e UndefinedBehaviorSanitizer
  (checagem de vptr e LeakSanitizer desativadas devido ao mock AML e ao executor).
- O workflow executa os testes e compila com Android NDK para ARM64.

Ainda precisa ser validado no aparelho: rosto/cabelo/acessórios obstruindo a câmera,
renderização de cabine, atraso entre câmera e animação, reflexos, skins modificadas e
entrada/saída de veículos. Caso a estrutura do esqueleto não seja válida, o mod mantém
a câmera nativa. Essa verificação não valida toda alteração feita por outro plugin.

Biblioteca alvo SHA-256:
`92b3714534742cf8e31eba35dbfd5e83871ecb5b936f5519d9b6ef3562234f9e`.

Se fechar, envie o novo crashlog. Se houver problema visual, envie um vídeo curto e
indique se acontece andando, olhando para baixo, entrando, dirigindo ou saindo.
Logs usam a tag `HeadCam` e a versão `0.2.1`.

## Compilar com NDK local

```sh
cmake -S firstperson -B build-firstperson \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build build-firstperson --parallel 2
```

Cabeçalhos AML e sua licença: `../mod/`.
