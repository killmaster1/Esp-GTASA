# Alyn Player ESP 0.3.3 — ARM64 / AML

Biblioteca experimental com ESP Box e ESP Line juntos. Desenha caixas verdes com contorno preto sobre os personagens identificados pelo GTA como jogadores e linhas do centro inferior da tela até as caixas. Ativa automaticamente quando o cliente Alyn compatível está carregado.

## Correção do crash na cor do texto (0.3.3)

Os logs enviados confirmam a 0.3.2 carregada e mostram um crash diferente: `SIGSEGV` em `CFont::SetColor + 4` (`libGTASA + 0x6cee10`), chamado por `simpleui::frame`. O registrador `x0` contém `0xffcde6f0`, exatamente os bytes RGBA da cor do texto `{240,230,205,255}`. A instrução nesse ponto é `ldrb w9,[x0]`: a função espera o endereço da cor, mas a declaração anterior passava os quatro bytes como um inteiro. O mesmo padrão de acesso existe em `SetDropColor`.

A 0.3.3 corrige as duas chamadas para receberem um ponteiro a uma cor local válida. A forma de passagem foi conferida no código ARM64 das funções; o nome C++ exportado, que mostra CRGBA por valor, não basta para reproduzir essa ABI com uma estrutura trivial. O desenho e os comandos continuam disponíveis. A integração pelo `BL` de `Idle`, introduzida na 0.3.2, é mantida.

O teste do menu agora executa também com `fontReady=true`, cobrindo as duas cores por ponteiro, a ordem da configuração de alpha, o texto do botão e do painel, o flush e a restauração dos 64 bytes de configurações da fonte. Anteriormente esse caminho não era exercitado. Compilar e passar nos testes do host não confirma o funcionamento no APK; a nova biblioteca ainda precisa de teste no aparelho.

## Correção do crash SIGILL na 0.3.2

O crash relatado na 0.3.1 ocorreu em `libGTASA.so + 0x4d89a4` (`Render2dStuff + 0x10`), durante o retorno do trampoline chamado pelo mod, antes do desenho do painel. A pilha inclui o wrapper Alyn `+0x47de10`, o mod e a entrada do GTA. O endereço é compatível com sobreposição entre patches na entrada, embora o log não contenha os bytes da instrução para provar qual patch estava presente. Não é evidência de proteção contra desenhos, nem de falta de arquivos de textura.

A 0.3.2 remove o nosso hook da entrada de `Render2dStuff`. Intercepta somente o `BL` em `Idle + 0x4d9108`, cujo destino original é `Render2dStuff` e cujo retorno é `+0x4d910c`. Usa `AML::HookBL`, disponível desde AML 1.0.4, preservando a entrada usada pelo cliente. A instrução esperada é `0x97fffe23`; se mudou ou o hook for rejeitado, desativa a integração e registra o motivo, sem voltar ao hook de entrada.

O botão e os comandos da 0.3.1 são mantidos, agora identificados como **ALYN 0.3.2**. O diagnóstico inclui `RENDER install=Idle_BL`, bytes da entrada antes da instalação, resultado do hook e fase `calling_original_2d` / `original_2d_returned`. O teste isolado verifica a instrução e que a entrada permanece intacta, inclusive quando já contém um patch. Isso não substitui a validação no aparelho.

**Force a parada completa do jogo antes de substituir `libsampesp.so` e inicie um processo novo**; trocar o arquivo com o processo aberto não desfaz os patches da versão anterior.

## Sniper na 0.3.0

- **TRAVA SNIPER**: ao abrir a luneta com a sniper equipada, escolhe o jogador válido mais próximo do centro da mira e acompanha a parte superior do tronco. Solte a mira para adquirir outro alvo. Considera obstáculos do cenário e a vida sincronizada.
- **ZOOM ESTENDIDO**: os controles normais da luneta passam a percorrer FOV de 70° até 1°. O alcance continua limitado à renderização atual do mapa e aos jogadores carregados pelo cliente.
- As duas opções iniciam ligadas e podem ser desligadas separadamente no menu. A atuação é suspensa enquanto o menu está aberto.

Detalhes, limites e análise das funções nativas: [SNIPER.md](SNIPER.md).

## Botão simples e comando local na 0.3.1

Substitui a janela ImGui pelo botão azul **MENU TESTE**, no alto ao centro, e um painel desenhado com `CSprite2d` + `CFont` do GTA. A legenda identifica a versão carregada (**ALYN 0.3.3** na correção atual). O botão apenas abre ou fecha o painel; não liga o ESP. O painel conserva controles separados para caixa, linha, vida, trava da sniper, zoom e consulta de jogadores com paginação.

No chat do SA-MP:

```text
/alynesp                 abre o menu (repetir mantém aberto)
/alynesp fechar          fecha
/alynesp teste           mostra confirmação visual
/alynesp box on          liga a caixa
/alynesp box off         desliga a caixa
/alynesp linha off       desliga a linha
/alynesp vida off        desliga a barra
/alynesp aim off         desliga a trava
/alynesp zoom off        desliga o zoom estendido
```

Todas as opções aceitam `on` e `off`. Os comandos desse namespace são interceptados na fila local do teclado; não são registrados como comandos de servidor. O ciclo normal de envio recebe uma string vazia para finalizar o teclado; o tratamento desse envio vazio ainda precisa de teste no cliente. Comandos de outros nomes e mensagens normais passam sem alteração e não são gravados no log. Não usa o comando `/esp`, que poderia conflitar com o servidor.

`alyn_esp.log` registra `SIMPLE_UI CMD /alynesp hook installed`, `TOUCH position/button hook installed`, `CMD RECEBIDO`, `TOQUE` e contadores `commands`, `presses`, `panel_frames`. Se o comando funcionar e o botão não, esses contadores ajudam a separar falha de entrada e de desenho. As assinaturas, entradas e layout de strings foram conferidos na biblioteca enviada. Há testes de fila, strings curtas/longas, passagem de chat, desenho, toque duplicado, seleção e opções. O APK Android ainda exige teste no aparelho.

O código da janela anterior e seus testes permanecem como referência; o hook `EndFrame` não é instalado na 0.3.1.

## Novidades da 0.2.0

- Barra vertical de vida à esquerda do ESP Box, preenchida de baixo para cima usando a vida sincronizada do cliente Alyn. A vida interna de 1000 usada pelo GTA para os remotos não é usada nessa barra.
- Cores: **verde acima de 75**, **amarelo acima de 50 até 75**, **laranja acima de 25 até 50**, **vermelho de 0 a 25**. Cinza indica falta de um valor sincronizado recente. A referência de preenchimento é 100; números maiores continuam disponíveis na lista.
- Na versão original, botão **MENU ESP** no alto, ao centro, com tema inspirado em San Andreas. Abre controles separados para **ESP Box**, **Barra de vida** e **ESP Line**. Desligar a caixa também oculta a barra; a linha tem controle independente. As opções valem durante a sessão e voltam ligadas ao reiniciar.
- Lista **Jogadores próximos**, ordenada por distância, com ID e vida. Selecione uma linha para mostrar também o colete. A seleção é somente para consulta, sem ação de dano.
- Mantém o arquivo de diagnóstico e a dependência mínima AML 1.3.1.

Detalhes da análise, offsets específicos e limites das conclusões sobre dano: [ANALISE.md](ANALISE.md).

## Instalação

1. No GitHub, abra **Actions → Build SA-MP ESP ARM64**, selecione uma execução concluída e baixe **SA-MP-ESP-ARM64** em Artifacts.
2. Extraia `libsampesp.so` e coloque na pasta de mods do AML usada pelo seu APK, com o jogo fechado.
3. Abra o cliente Alyn e entre no servidor.

Requer AML 1.3.1 ou superior carregado pelo APK do SA-MP, além das bibliotecas compatíveis abaixo. Apenas colocar a `.so` no aparelho não instala o AML. Não substitua `libGTASA.so` nem `libAlyn_SAMPMOBILE.so`. Para desativar, remova somente `libsampesp.so` com o jogo fechado.

## Comportamento

- Box 2D calculado pela cabeça, quadril, mãos e pés; verifica a presença dos ossos antes da consulta.
- Line de baixo da tela até a parte inferior da caixa.
- Alcance de 250 metros e limite de 256 alvos desenhados por quadro.
- Exclui o personagem local, pedestres comuns, personagens mortos, slots livres, outras áreas/interiores e alvos atrás da câmera.
- Não testa oclusão por paredes. Alvos parcialmente fora da tela têm as linhas recortadas antes do desenho.
- Só funciona com personagens que já estejam carregados no cliente. Não revela jogadores fora do streaming, não consulta posições no servidor e não altera pacotes de rede.
- A seleção usa `CPed::IsPlayer` do GTA (tipos 0 e 1), e não uma lista de IDs extraída de `CPlayerPool` do SA-MP. Bots ou personagens de outros mods classificados como jogadores pelo GTA também podem aparecer; isso precisa ser conferido no servidor usado.
- O menu usa desenho e fonte do GTA. O toque é observado nos eventos de entrada do ImGui do cliente, sem depender de janelas ImGui. Não bloqueia controles subjacentes; possíveis conflitos precisam de teste no aparelho. O comando local oferece um caminho independente do toque.

## Compatibilidade analisada

Cliente enviado: `libAlyn_SAMPMOBILE.so`, ELF AArch64, NDK r27b, Android API 24. Build ID: `a861944204c6309bb433b7e2be8a2f13c137ae66`. SHA-256: `d8e112ed6543c2fa4ec73560a8b141334f27df1f06de728f9da57bc867ddc160`.

GTA: a `libGTASA.so` 2.10 ARM64 enviada anteriormente, SHA-256 `92b3714534742cf8e31eba35dbfd5e83871ecb5b936f5519d9b6ef3562234f9e`. As estruturas do GTA foram verificadas nos dados DWARF. O mod confere os símbolos e seus endereços relativos e aguarda o cliente com o Build ID indicado. Outro GTA ou outra compilação Alyn exige nova análise.

O cliente tem os símbolos internos de jogadores removidos; esta versão não usa offsets presumidos de `CNetGame` ou `CPlayerPool`. A biblioteca original do cliente não é modificada nem distribuída neste projeto.

## Correção da dependência AML (0.1.2)

O `.ini` enviado indica `Version=1.3.1`. As versões 0.1.0 e 0.1.1 declaravam AML 1.4.0 como mínimo, o que pode fazer o carregador rejeitar o mod antes dos callbacks e da criação de `alyn_esp.log`. A 0.1.2 aceita AML 1.3.1: `GetSym`, `Hook` e `GetConfigPath` existem desde 1.0.0.0; `GetDataPath` desde 1.0.0.4; `GetAndroidDataPath` desde 1.0.0.5; `GetLibHandle` desde 1.0.1, conforme os cabeçalhos AML usados. A compatibilidade efetiva no APK ainda deve ser conferida no aparelho.

Para habilitar também o log do carregador, com o jogo fechado, edite **somente estas entradas existentes** da seção `[Preferences]` do `.ini` do AML:

```ini
PrintLogsToFile=1
PrintLogsToFile_NoLogCat=0
PrintLogsToFile_Verbose=1
```

Mantenha `EnableLogcats=1` e `DontLoadMods=0`, que já estão assim no arquivo enviado. Não altere manualmente `Version=1.3.1`: isso não atualiza a biblioteca AML. Reinicie o cliente após trocar a `.so` e salvar o `.ini`.

Se ainda não houver `alyn_esp.log`, envie os arquivos de log gerados pelo AML e o caminho completo onde `libsampesp.so` foi colocado. O `.ini` pode ser antigo, então seu conteúdo sozinho não comprova a versão efetivamente carregada.

## Diagnóstico por arquivo (0.1.1)

A `.so` tenta gravar **alyn_esp.log** nestas pastas fornecidas pelo próprio AML, na ordem:

1. Pasta externa do aplicativo: normalmente `Android/data/<pacote-do-SA-MP>/files/alyn_esp.log`.
2. Pasta de configurações do AML: `alyn_esp.log` dentro dela.
3. Pasta de dados internos do aplicativo: alternativa caso as anteriores não permitam gravar.

A pasta exata depende do APK e das configurações do AML. O caminho efetivamente usado também aparece no logcat, tag `AlynESP`. Se todas as tentativas falharem, os erros de abertura, com caminho e `errno`, ficam no logcat. O mod não pede permissões adicionais nem grava fora das pastas fornecidas pelo AML.

Substitua a `.so` antiga com o jogo fechado, abra o cliente, entre no servidor, fique cerca de **30 segundos perto de outros jogadores**, feche normalmente o jogo e envie `alyn_esp.log`. O arquivo é atualizado durante a execução, com flush após cada evento. Ao atingir aproximadamente 512 KiB, a versão anterior vira `alyn_esp.log.previous`; são mantidos o arquivo atual e uma cópia anterior.

O diagnóstico registra:

- Entrada em `OnModLoad` e `OnAllModsLoaded`, bibliotecas carregadas e símbolos ausentes ou com endereço relativo divergente.
- Caminho e Build ID do cliente encontrado, incluindo divergência da versão esperada.
- Instalação do hook. Um monitor independente registra a cada cinco segundos se o callback de desenho foi chamado. Zero chamadas também pode ocorrer em menus, com jogo pausado ou sem renderização, portanto isoladamente não prova conflito de hooks.
- A cada cinco segundos, o motivo de interrupção e contadores de slots, tipos de pedestre, candidatos a jogador, descartes, caixas e chamadas ao desenho.
- Motivos dos descartes: vida, distância, interior, ausência de modelo ou hierarquia, osso ausente, posição inválida, projeção e alvo fora da tela.

Como ler os primeiros sinais:

| Registro | O que investigar |
| --- | --- |
| `required_symbol_missing` / `gta_layout_mismatch` | A `libGTASA.so` em execução difere da analisada. |
| `client_not_verified` | Biblioteca do cliente ausente, com outro nome, ou Build ID diferente. |
| `hook_installed` e nenhum callback durante a partida | Função de desenho não executada ou hook substituído; o log sozinho não determina a causa. |
| `players=0` com slots ocupados e `not_player` alto | A classificação `CPed::IsPlayer` pode não corresponder aos jogadores desse cliente. |
| `missing_bone`, `no_hierarchy` ou `projection_failed` altos | Os candidatos foram encontrados, mas a caixa não passou pelas verificações. |
| `drawn` e `polygon_calls` positivos sem imagem | O código chegou às chamadas de desenho; investigar estados gráficos, ordem de renderização ou sobreposição. Isso não comprova que a GPU exibiu as figuras. |

Se **nenhum arquivo aparecer**, confira também as pastas alternativas. Pode ser que o AML não tenha carregado esta `.so`, que a dependência não tenha sido atendida ou que nenhum caminho tenha permitido gravação. Uma biblioteca que não foi carregada não consegue gerar seu próprio arquivo: nesse caso, é necessário o log do AML ou o logcat.

Os testes simulados cobrem filtro de personagens, ossos ausentes, caixa e linha, clipping, identificação ELF e motivos de rejeição. Também verificam as quatro faixas de vida, expiração e associação do cache, diferença entre vida sincronizada e vida GTA 1000, controles reais do desenho, seleção na lista e equilíbrio das pilhas do menu. Os testes de arquivo verificam fallback após falha de abertura, flush antes de fechar, gravações concorrentes e rotação. Esses testes não substituem a execução no celular. Nenhuma verificação de versão nem filtro de desenho foi removido nesta atualização.

## Compilar

A ação `.github/workflows/sampesp.yml` executa os testes e gera `build-sampesp/libsampesp.so` com NDK 27.2, ABI `arm64-v8a`, Android API 23. O cliente analisado requer Android API 24.

Cabeçalhos AML compartilhados em `../mod/`; licença em `../mod/LICENSE`.
