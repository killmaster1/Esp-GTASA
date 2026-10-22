# Investigação de jogadores fora do streaming — 0.3.8

Biblioteca Alyn ARM64 fornecida, Build ID `a861944204c6309bb433b7e2be8a2f13c137ae66`. Endereços relativos a essa compilação. A análise é estática; os testes do observador são simulados. O recebimento de coordenadas no servidor real ainda não foi confirmado.

## Caminho confirmado: pacote compacto de posições

O dispatcher compara o ID de pacote com 208 em `0x4e68cc`; o ramo em `0x4e68d0` alcança a chamada em `0x4e698c` para o handler `0x4e75a0`. O handler lê primeiro oito bits e depois um contador de 32 bits, aceitando de 1 a 1004 entradas. Em cada entrada lê ID de 16 bits e um bit de atividade. Quando ativo, lê três componentes de 16 bits em `0x4e76a8..0x4e76e0`.

O acesso ao jogador é feito pelo conjunto de pools em `netgame + 1376`, primeiro ponteiro de pool. Os IDs são limitados a 1003, com uma flag de slot em `pool + 8040 + id`; o ponteiro de entrada está em `pool + 8 + id*8`, e o primeiro ponteiro dessa entrada fornece o remoto. Isso demonstra uma relação de IDs separada do pool GTA de personagens. Uma entrada de jogador não implica que sua posição esteja disponível.

O handler chama `0x501ec0` em `0x4e7724`, passando remoto e três inteiros. Nessa função, `sxth` converte os componentes para inteiros de 16 bits com sinal antes da conversão para float. Portanto, são coordenadas em unidades inteiras, com menos precisão que a sincronização normal por floats. O ID do remoto é lido em +182. O wrapper GTA é lido em +288.

A função marca `remote + 304`, lida com o objeto de radar em +296 e cria o marcador nativo quando não existe wrapper em +288. Isso é evidência de um caminho concebido para posições de jogadores sem personagem GTA carregado. Os três valores não são gravados em um campo de posição exata do remoto nesse trecho; são usados na chamada que cria o objeto de radar. Por isso, presumir que existe um vetor XYZ atualizado no CRemotePlayer seria incorreto.

Quando a entrada está inativa, o handler chama `0x501f80` a partir de `0x4e7648`. A função remove o objeto de radar e limpa a flag +304. O pacote não fornece interior ou mundo virtual nessa representação.

## Sincronização normal e último local conhecido

O processamento remoto em `0x502060` verifica estado e wrapper. No caminho de remoção chama `0x5016e0`, remove o wrapper e zera `remote + 288`. A rotina `0x5016e0` também limpa áreas que guardam sincronização normal em +26, +42, +58, +74 e outras. Não é seguro depender desses campos como histórico permanente após o jogador sair do streaming.

Um cache próprio de última posição exata pode preservar dados recebidos antes da remoção, desde que registre horário, origem e sessão. Ele permite estimar onde o jogador foi visto, sem provar onde está agora. Reutilização de ID, reconexão e troca de interior/mundo exigiriam invalidação. Esta investigação não usa esse histórico para mover o jogador.

## Observador entregue

A 0.3.8 intercepta somente as chamadas existentes em `0x4e7724` e `0x4e7648` com `AML::HookBL`, após a verificação do Build ID. Confere as instruções `0x940069e7` e `0x94006a4e`. Encaminha uma vez a chamada original, com os mesmos argumentos. Não chama novas funções de radar, não solicita novas informações ao servidor e não altera teleporte ou streaming. Marcadores que o próprio cliente já criava mantêm o comportamento original.

Copia ID, coordenadas com sinal, presença de wrapper e horário para um cache de diagnóstico limitado a 1004 slots. Invalida a amostra ao receber o estado inativo e considera recente somente a faixa de cinco segundos. O cache não é uma fonte de autorização ou um destino de teleporte: não acompanha todos os motivos de remoção de jogador ou mudança de sessão e não conhece o mundo/interior. A biblioteca registra `DISTANT_SAMPLE`, `DISTANT_INACTIVE` e o resumo `DISTANT_PROBE`.

As amostras no arquivo são limitadas a 24 linhas por segundo e normalmente uma linha por ID a cada dez segundos, além de transições. O resumo usa todas as atualizações válidas recebidas. `without_wrapper_updates` conta atualizações, não jogadores únicos; `fresh_without_wrapper` conta IDs com amostra recente e sem wrapper na última atualização. Não atesta a distância ou continuidade posterior da posição.

## Outras possibilidades e seus limites

| Caminho | Utilidade | Limite |
| --- | --- | --- |
| Posição compacta recebida, sem desenhar marcador adicional | Encontrar a região atual aproximada de um jogador distante | Depende do envio pelo servidor; precisão inteira; sem interior/mundo |
| Histórico de sincronização normal | Voltar ao último lugar onde o jogador foi visto | Pode ficar antigo assim que ele sai do streaming; exige invalidações |
| ID no pool de jogadores conectados | Identificar um jogador que não tem CPed carregado | Não revela coordenadas sozinho |
| Modo espectador autorizado pelo servidor | O servidor pode passar a fornecer a região do alvo | Mudar somente a câmera local não obriga o servidor a enviar o jogador |
| Comando administrativo ou integração no próprio servidor | Obter posição exata, interior e mundo e fazer um teleporte coerente | Exige implementar/verificar permissões no servidor |

Não foi demonstrado um pedido universal, disponível apenas no cliente, que obtenha a coordenada exata de qualquer ID distante. Não se deve confundir suporte a um pacote com prova de que o servidor o envia para todos os jogadores.

## Teste sugerido

Com cliente em LV e um segundo jogador em LS, mova o segundo durante cerca de 30 segundos e consulte `DISTANT_SAMPLE` para seu ID. Compare mais de uma amostra e as coordenadas esperadas. Uma amostra recente com `remote_has_wrapper=0` demonstra recebimento desse dado sem um personagem GTA associado. Se houver apenas amostras quando próximo, o teste não confirma teleporte distante por esse caminho. Envie o log e o ID do segundo jogador para interpretar o resultado. Não é necessário adicionar marcador ao mapa.
