# Trava da sniper e zoom estendido — 0.3.0

No menu **MENU ESP**, as opções **TRAVA SNIPER** e **ZOOM ESTENDIDO** iniciam ligadas e têm controles independentes.

## Usar

Equipe a sniper (arma GTA 34) e abra a luneta. A trava escolhe o jogador carregado cuja parte superior do tronco está mais próxima do ponto central da mira. Mantém esse alvo enquanto ele permanecer válido e com linha de visão; solte a mira para escolher outro. O disparo continua no controle do jogador.

A aquisição considera o centro da tela, não a distância no mapa. Personagens mortos, NPCs comuns, outras áreas e alvos encobertos por construções, veículos ou objetos são descartados. A trava atua nas câmeras da luneta 7 e 46, com a sniper equipada e a câmera pertencendo ao jogador local. Fica suspensa durante o menu, espelhos e câmeras de cena.

Use os controles normais de aproximar/afastar da sniper para o zoom. A nova faixa é **FOV de 70° a 1°**; quanto menor o FOV, maior a aproximação. O limite de 1° é o piso adotado por este mod para manter uma projeção positiva e finita, não uma garantia de máximo absoluto de ampliação que o hardware ou o motor conseguiriam representar.

O alcance acompanha o plano distante atual da câmera do jogo (`farPlane`). O zoom aproxima o cenário disponível nessa renderização. Jogadores continuam dependendo do streaming do cliente/servidor; a resolução dos objetos distantes e seus LODs continuam sendo os disponíveis no jogo. A distância de tiro depende da arma e das regras do servidor.

## Implementação conferida na biblioteca ARM64 enviada

- `CCam` tem 552 bytes; modo em 14, Alpha em 132, FOV em 140, Beta em 148, Front em 360, Source em 372 e Up em 396. A câmera ativa é obtida em `TheCamera + 376 + ActiveCam * 552`, com índice em 95.
- A seleção e os vetores são aplicados após o processamento original de `CCam::Process` (`0x49eebc`), antes de a função externa da câmera construir suas matrizes normais.
- A rotina nativa de primeira pessoa da luneta (`0x4a3ea8`) usa `Front = (-cos(Beta)*cos(Alpha), -sin(Beta)*cos(Alpha), sin(Alpha))`. O mod escreve os ângulos correspondentes à direção do peito do alvo.
- O slot de arma ativo é lido em CPed + 2268; os 13 slots começam em 1840 e cada `CWeapon` tem 32 bytes, com tipo em 0. Esses layouts foram conferidos nos dados DWARF da biblioteca fornecida.
- A aquisição usa os vetores da câmera atual para calcular a proximidade ao centro, evitando usar uma matriz de projeção do quadro anterior. O alvo inicial precisa estar dentro do campo de visão. Um alvo já travado pode ser acompanhado ao sair brevemente dos limites da tela, enquanto permanecer à frente e válido.
- A linha de visão usa a função nativa `CWorld::GetIsLineOfSightClear`. Pedestres são ignorados no raio para o próprio alvo não bloquear a consulta; construções, veículos, objetos e dummies continuam considerados.
- O valor de vida sincronizada também é usado quando disponível: um remoto com CPed em 1000 e vida sincronizada em zero não é adquirido.
- `CDraw::SetFOV` (`0x6c9c88`) acrescenta uma correção de tela larga. Para o pedido de FOV da própria luneta estendida, o valor final é ajustado para o FOV positivo escolhido. Pedidos diferentes e outras câmeras mantêm o processamento original.
- `CPad::SniperZoomIn/Out` fornece os controles; o passo temporal do jogo controla a progressão exponencial e o FOV é limitado ao intervalo 1..70.

Os 17 símbolos adicionais são conferidos com seus endereços relativos antes dos hooks. A instalação usa as mesmas APIs AML compatíveis com 1.3.1. Bibliotecas ou mods que mudem a câmera podem exigir adaptação.

## Diagnóstico e validação

`alyn_esp.log` inclui `SNIPER installed`, arma, modo, estado de luneta, ID do alvo quando associado à sincronização e FOV. `target_id=-1` pode significar ausência de alvo ou alvo sem associação recente no cache.

Os testes cobrem escolha pelo centro da mira, obstrução, far plane, jogador morto na sincronização, osso ausente, arma incorreta, abertura do menu, espelho, saída da luneta, limites de zoom, pedidos de FOV alheios e retorno ao processamento nativo. Os controles novos também entram no teste de toque com ImGui real no host.

Esses testes e a compilação Android não substituem o teste do alinhamento da luneta e do zoom no aparelho.
