# Alyn Player ESP 0.3.8 — ARM64 / AML

Biblioteca experimental com ESP Box e ESP Line juntos. Desenha caixas verdes com contorno preto sobre os personagens identificados pelo GTA como jogadores e linhas do centro inferior da tela até as caixas. Ativa automaticamente quando o cliente Alyn compatível está carregado.

## Investigação de posições distantes (0.3.8)

Esta versão adiciona somente um observador dos dados de posição compactada que o cliente já recebe. Não adiciona marcador, menu, comando ou teleporte. Preserva as funções da 0.3.7. O objetivo é verificar se o servidor envia posições recentes de jogadores sem personagem carregado no cliente.

As linhas `DISTANT_SAMPLE` em `alyn_esp.log` identificam jogador, coordenadas inteiras, presença do wrapper GTA e origem do dado. `remote_has_wrapper=0` significa que, no momento do pacote, o objeto remoto não tinha wrapper de personagem. Isso não informa por si só a distância, o interior ou o mundo virtual. O relatório `DISTANT_PROBE` resume os contadores. Logs de amostras são limitados por ID e globalmente; o cache de diagnóstico recebe todas as amostras válidas e considera recentes apenas as dos últimos cinco segundos.

Para testar: deixe um jogador em LS e o cliente de teste em LV, mova o jogador de LS durante cerca de 30 segundos e consulte/envie o `alyn_esp.log`, informando seu ID. Uma posição recente sem wrapper comprova esse caminho para o jogador no servidor testado; ausência de amostra ainda pode significar configuração do servidor, jogador sem entrada ativa ou caminho diferente do cliente. Isso não torna uma posição antiga válida para teleporte.

Análise e alternativas: [POSICOES_DISTANTES.md](POSICOES_DISTANTES.md).

## Dano da sniper em personagens em 3× (0.3.7)

O dano base dos impactos nativos da sniper local contra personagens é multiplicado por três. Exemplo: 100 passa a 300, antes dos modificadores normais do jogo. Mantém cadência nominal 2×, penetração, ESP e ausência de menu. Não muda dano de veículos/objetos, outras armas ou disparos de outros jogadores. O servidor continua podendo recalcular ou rejeitar dano remoto.

Intercepta apenas a chamada nativa de `CBulletInfo::Update` para `CWeapon::GenerateDamageEvent` em `0x6fc8a0`, instrução `0x97ec753c`, preservando a entrada da função. Confere os símbolos e aplica o multiplicador somente com cliente verificado, dono igual ao jogador local, arma 34 e valor positivo. Encaminha uma única chamada original com os mesmos alvo, dono, parte do corpo e direção, preservando o retorno. Não implementa RPCs, tiros extras ou escrita direta na vida. Não modifica os slots da bala, evitando multiplicação repetida ao atravessar vários quadros. Valores que estourariam `int` são limitados ao máximo positivo.

O log `DAMAGE` contém instalação, multiplicador, impactos ajustados e último valor base/final. Os testes verificam 3×, uma chamada por impacto, parâmetros/retorno, tipos de arma/dono, cliente não verificado, zero/negativos e overflow. Exige validação no aparelho.

## Cadência nominal da sniper em 2x (0.3.6)

Após um disparo válido da sniper local, o intervalo restante para o próximo tiro é reduzido à metade. Exemplo: 1000 ms passa a 500 ms. A redução ocorre uma vez por disparo, incluindo o temporizador de recarga iniciado ao esvaziar o pente. Não altera dano nem munição, não gera tiros extras e não afeta outras armas ou jogadores. Uma recarga manual separada não é acelerada.

Usa os pontos de chamada de `CWeapon::Fire` em `CTaskSimpleUseGun::FireGun` (`0x5dff14`, instrução `0x97f1229f`) e `CTaskSimplePlayerOnFoot::ProcessPlayerWeapon` (`0x6536ec`, `0x97ef54a9`), através de `AML::HookBL`. A entrada de `Fire` é preservada. O DWARF confirma `CWeapon` de 32 bytes com tipo em 0, estado em 4, munição em 8/12 e temporizador em 16. O ajuste ocorre depois do retorno original bem-sucedido, somente se houve consumo de munição e o estado é disparando ou recarregando. O contador de tempo do jogo é `CTimer::m_snTimeInMilliseconds` em `0xbdc558`.

O log inclui `CADENCE hooks`, `shots_adjusted`, `original_ms` e `reduced_ms`. Esses valores permitem conferir se o caminho é usado pelo APK e a redução aplicada. A cadência real ainda depende de limites adicionais de animação, controles e servidor. Os testes cobrem redução única, munição preservada, disparo falho, filtro de arma/dono, último tiro do pente e overflow do relógio. O funcionamento no aparelho ainda precisa ser validado.

## Sniper atravessando o cenário (0.3.5, experimental)

A rotina local de colisão das balas da sniper do jogador passa a ignorar construções, objetos e dummies. Personagens e veículos continuam na consulta. Ativa automaticamente no cliente compatível, sem menu. O dano nativo da arma, munição, tempo de vida da bala, efeitos e processamento de acerto permanecem os originais. Não altera o servidor nem cria envios artificiais de dano.

A implementação ignora essas categorias por completo; não calcula espessura nem reduz dano pela parede. O alcance continua dependente da bala nativa e do streaming. A trava automática da sniper continua exigindo linha de visão para adquirir um alvo; essa versão muda a colisão do disparo. O efeito no dano de jogadores remotos depende da validação do servidor.

Análise, ponto de instalação e diagnóstico em [PENETRACAO.md](PENETRACAO.md). Os testes são simulados e a biblioteca foi compilada localmente com NDK r27c. Ainda exige validação no APK.

## Versão 0.3.4 sem menu

Removidos os menus antigo e simples, o botão, os textos de interface, a lista de jogadores e os hooks de toque/teclado. Não há comando `/alynesp` nesta versão. Não chama `CFont` nem funções ImGui. O ESP Box, ESP Line, barra de vida, trava da sniper e zoom estendido continuam iniciando ligados, automaticamente, no cliente compatível. Para desativar o mod inteiro, remova a biblioteca com o jogo encerrado.

O hook de renderização continua no `BL` de `Idle + 0x4d9108`, preservando a entrada de `Render2dStuff`. O log `alyn_esp.log` continua disponível, com versão, carregamento, saúde sincronizada, renderização e estado da sniper.

## Compilar localmente no Linux

Com Android NDK r27c instalado, não é necessário usar GitHub Actions, Gradle ou SDK completo:

```sh
bash sampesp/build_local.sh /caminho/android-ndk-r27c /caminho/saida
```

O script usa o compilador do NDK para **Android ARM64, API 23**, vincula a biblioteca C++ estaticamente e gera `libsampesp.so`. O download do NDK é necessário apenas na primeira preparação do ambiente. As compilações seguintes reutilizam essa instalação. O workflow do GitHub continua disponível como alternativa.

A rotina da sniper e seus limites estão em [SNIPER.md](SNIPER.md). A análise do cliente está em [ANALISE.md](ANALISE.md); as partes referentes à interface descrevem versões antigas.

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
- Esta versão não possui interface de menu, botão ou interceptação do chat/toque.

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
