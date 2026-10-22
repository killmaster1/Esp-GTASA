# Alyn Player ESP 0.1.0 — ARM64 / AML

Biblioteca experimental com ESP Box e ESP Line juntos. Desenha caixas verdes com contorno preto sobre os personagens identificados pelo GTA como jogadores e linhas do centro inferior da tela até as caixas. Ativa automaticamente quando o cliente Alyn compatível está carregado.

## Instalação

1. No GitHub, abra **Actions → Build SA-MP ESP ARM64**, selecione uma execução concluída e baixe **SA-MP-ESP-ARM64** em Artifacts.
2. Extraia `libsampesp.so` e coloque na pasta de mods do AML usada pelo seu APK, com o jogo fechado.
3. Abra o cliente Alyn e entre no servidor.

Requer AML 1.4.0 ou superior carregado pelo APK do SA-MP, além das bibliotecas compatíveis abaixo. Apenas colocar a `.so` no aparelho não instala o AML. Não substitua `libGTASA.so` nem `libAlyn_SAMPMOBILE.so`. Para desativar, remova somente `libsampesp.so` com o jogo fechado.

## Comportamento

- Box 2D calculado pela cabeça, quadril, mãos e pés; verifica a presença dos ossos antes da consulta.
- Line de baixo da tela até a parte inferior da caixa.
- Alcance de 250 metros e limite de 256 alvos desenhados por quadro.
- Exclui o personagem local, pedestres comuns, personagens mortos, slots livres, outras áreas/interiores e alvos atrás da câmera.
- Não testa oclusão por paredes. Alvos parcialmente fora da tela têm as linhas recortadas antes do desenho.
- Só funciona com personagens que já estejam carregados no cliente. Não revela jogadores fora do streaming, não consulta posições no servidor e não altera pacotes de rede.
- A seleção usa `CPed::IsPlayer` do GTA (tipos 0 e 1), e não uma lista de IDs extraída de `CPlayerPool` do SA-MP. Bots ou personagens de outros mods classificados como jogadores pelo GTA também podem aparecer; isso precisa ser conferido no servidor usado.
- Não há menu, botão, nome, ID ou alteração de mira nesta versão.

## Compatibilidade analisada

Cliente enviado: `libAlyn_SAMPMOBILE.so`, ELF AArch64, NDK r27b, Android API 24. Build ID: `a861944204c6309bb433b7e2be8a2f13c137ae66`. SHA-256: `d8e112ed6543c2fa4ec73560a8b141334f27df1f06de728f9da57bc867ddc160`.

GTA: a `libGTASA.so` 2.10 ARM64 enviada anteriormente, SHA-256 `92b3714534742cf8e31eba35dbfd5e83871ecb5b936f5519d9b6ef3562234f9e`. As estruturas do GTA foram verificadas nos dados DWARF. O mod confere os símbolos e seus endereços relativos e aguarda o cliente com o Build ID indicado. Outro GTA ou outra compilação Alyn exige nova análise.

O cliente tem os símbolos internos de jogadores removidos; esta versão não usa offsets presumidos de `CNetGame` ou `CPlayerPool`. A biblioteca original do cliente não é modificada nem distribuída neste projeto.

## Diagnóstico e testes

Tag de logcat: `AlynESP`.

- `Alyn ESP 0.1.0 ready`: hook do GTA instalado, aguardando o cliente compatível.
- `Alyn client build verified`: cliente encontrado, desenho habilitado.
- `Unsupported GTA library layout` ou `Required GTA symbol missing`: GTA incompatível, mod desativado.

Os testes com memória e funções simuladas cobrem filtro de personagens, ossos ausentes, caixa e linha, alvos parcialmente visíveis, coordenadas inválidas, ausência do cliente e identificação ELF. Não comprovam a classificação dos jogadores neste servidor nem a execução no aparelho. A compilação NDK também precisa ser seguida de teste real no celular.

## Compilar

A ação `.github/workflows/sampesp.yml` executa os testes e gera `build-sampesp/libsampesp.so` com NDK 27.2, ABI `arm64-v8a`, Android API 23. O cliente analisado requer Android API 24.

Cabeçalhos AML compartilhados em `../mod/`; licença em `../mod/LICENSE`.
