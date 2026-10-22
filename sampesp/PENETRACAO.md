# Penetração local da sniper — 0.3.5

O alvo desta alteração é o cálculo de colisão do disparo no jogo. A biblioteca do cliente multiplayer e o servidor não são modificados.

## Fluxo confirmado na libGTASA ARM64 enviada

`CWeapon::FireSniper` (`0x702b20`) chama `CBulletInfo::AddBullet` em `0x702c6c`. A posição e a velocidade da bala são registradas em `gaBulletInfo`, com oito slots. O DWARF identifica o array em `0xd28a08`, stride de 56 bytes: arma em 0, dono em 8, expiração em 16, ativo em 20, posição em 24, velocidade em 36 e dano em 48.

`CBulletInfo::Update` (`0x6fc638`) copia a posição do slot para o início do segmento e escreve seu dono em `CWorld::pIgnoreEntity`. A chamada em `0x6fc7f8` é `BL CWorld::ProcessLineOfSight` através da PLT, instrução `0x97eca0d2`. O endereço do array é conferido nas instruções em `0x6fc6e0`, além da conferência dos símbolos exportados e offsets. Uma incompatibilidade desativa somente esta função, sem usar hook de entrada como alternativa.

O mod intercepta apenas essa chamada com `AML::HookBL`. Aplica o filtro quando o cliente está verificado, o dono ignorado é o jogador local e existe um slot ativo da arma 34 com esse mesmo dono e posição exatamente igual à origem do segmento. A identificação usa a própria bala, e não a arma que estiver equipada posteriormente. Se outra bala ativa do mesmo dono e de outra arma tiver a mesma origem, mantém as colisões originais para evitar uma identificação ambígua.

Nessa consulta, os parâmetros `buildings`, `objects` e `dummies` são falsos. Preserva os parâmetros de veículos, personagens e demais flags, os extremos do segmento e os buffers de saída. O processamento nativo continua a partir do resultado, incluindo dano e efeitos. Não chama diretamente eventos de dano, não edita a vida dos alvos e não implementa RPCs.

## Limites e teste no aparelho

O filtro atravessa todas as construções e objetos considerados nessa consulta, incluindo partes do terreno representadas por essas categorias. Não calcula número ou espessura de paredes. Veículos continuam bloqueando se habilitados pela consulta original. Colisões do personagem e de outras armas não são alteradas. O aimlock mantém sua exigência de visibilidade.

Se o cliente substituir integralmente a simulação da bala ou ignorar os flags da consulta, o efeito pode não aparecer. O servidor também pode rejeitar o dano remoto. O comportamento efetivo precisa ser conferido no APK, usando primeiro um alvo local atrás de uma parede.

`alyn_esp.log` registra `PENETRATION installed` ou o motivo da desativação. O relatório periódico contém `bullet_rays`, `filtered_local_sniper` e `filtered_hits`. O primeiro conta consultas que passaram pelo hook; o segundo, consultas que receberam o filtro; o terceiro, resultados positivos dessas consultas. Não são contadores de paredes atravessadas nem prova de dano confirmado pelo servidor.

Os testes verificam parede e objeto antes de um alvo simulado, veículos sólidos, outros donos e armas sem alteração, cliente não verificado, posição diferente, slot inativo, flags de personagem preservados, buffers nativos de saída e campos de dano/expiração intactos. O teste do instalador rejeita uma instrução de chamada diferente ou uma falha do hook. Esses testes não executam o GTA.
