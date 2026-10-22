# NPC Lines — GTA SA 2.10 ARM64 / AML

Projeto experimental em C++; não inclui uma biblioteca Android compilada.
Destinado à libGTASA.so enviada nesta conversa. CLEO não é necessário para este mod.

## Comportamento

Linhas verdes com 1,5 pixel, do centro inferior da tela até NPCs vivos a até
100 metros, limitadas a 128 linhas por quadro. Exclui o jogador. Alvos atrás
da câmera ou fora da tela são ignorados; paredes não bloqueiam as linhas.
Não há botão de ativação: o mod fica ativo enquanto instalado.

## Compilar no computador com Android NDK

Instale Android NDK e CMake. Na pasta deste projeto, em um terminal bash:

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

O resultado será `build/libnpclines.so`.

## Compilar pelo GitHub

O GitHub Actions está configurado em `.github/workflows/build.yml`.
Abra Actions → Build ARM64 → Run workflow → Run workflow.
A compilação também inicia quando o código muda na branch main.
Quando a execução terminar com sucesso, abra a execução e, em Artifacts,
baixe NPC-Lines-ARM64. Extraia `libnpclines.so` do ZIP.
Não é necessário instalar o NDK no celular.

O resultado de cada compilação aparece na aba Actions; a configuração do
workflow não confirma funcionamento dentro do jogo.

## Instalar e testar

Coloque `libnpclines.so` na mesma pasta de mods `.so` usada pelo seu AML.
O caminho depende da configuração do APK; não substitua `libGTASA.so`.
Feche e abra o jogo e carregue um save com pedestres próximos.
Para desativar, remova somente `libnpclines.so` com o jogo fechado.
Mensagens de inicialização usam a tag `NPCLines` no logcat.

Se nenhuma linha aparecer, confira o log: o mod se desativa quando os símbolos
ou seus endereços relativos divergem dos verificados na biblioteca enviada.
Essa checagem não equivale a validar todas as alterações de outros plugins.

## Validação realizada e limites

- ELF da biblioteca enviada: AArch64 / 64 bits.
- Símbolos de HUD, projeção, desenho de triângulos e acesso a pedestres confirmados.
- Layouts de CPlaceable, CPed e CPool conferidos nos dados DWARF da biblioteca.
- Sintaxe de main.cpp validada com GCC do host, em C++14, usando declarações
  mínimas de JNI/Android só para essa checagem. Esses arquivos temporários não
  fazem parte do projeto. Isso NÃO valida a compilação Android ou a ABI em execução.
- A compilação NDK é verificada pelo workflow; veja o resultado na aba Actions.
- Instalação, interação com FLA/CLEO e funcionamento no aparelho ainda NÃO testados.

O acesso aos NPCs usa CPools::GetPed para respeitar alterações de alocação do FLA.
Offsets de posição e vida continuam específicos da biblioteca enviada; não há
compatibilidade garantida com modificações adicionais dessas estruturas.

Biblioteca analisada, SHA-256:
`92b3714534742cf8e31eba35dbfd5e83871ecb5b936f5519d9b6ef3562234f9e`

Cabeçalhos AML em `mod/` obtidos do template oficial em:
https://github.com/AndroidModLoader/AndroidModLoader/tree/main/template_of_mod/mod
Os cabeçalhos atuais incluem métodos de AML 1.4.1; este projeto chama somente
métodos disponíveis antes dessa versão e declara dependência mínima de AML 1.4.0.
Licença dos cabeçalhos: `mod/LICENSE`.
