# Análise da biblioteca Alyn ARM64 para vida e interface

Biblioteca analisada: `libAlyn_SAMPMOBILE.so`, Build ID `a861944204c6309bb433b7e2be8a2f13c137ae66`, SHA-256 `d8e112ed6543c2fa4ec73560a8b141334f27df1f06de728f9da57bc867ddc160`. Os endereços abaixo são relativos à base dessa compilação específica. Não são offsets universais de SA-MP.

## Vida visual e vida sincronizada são campos diferentes

O getter nativo do wrapper de personagem, em `0x4590f0`, acessa a entidade GTA em `wrapper + 312` e lê sua vida em `CPed + 1708`.

Na rotina identificada como processamento de jogador remoto, intervalo `0x502060..0x50276c`, há uma chamada ao setter do wrapper em `0x5026d0` que atribui **1000.0** à entidade GTA, exceto em determinado estado. Portanto, usar somente a vida do CPed poderia produzir uma barra cheia mesmo quando o jogador remoto recebeu outro valor pela rede.

A rotina de criação associa o wrapper ao objeto remoto em `remote + 288`. O ID é lido em `remote + 182` durante essa criação. Os handlers de sincronização fazem a conversão da vida e do colete de bytes para floats e gravam o par em `remote + 16` e `remote + 20`:

- Em `0x50341c..0x50342c`, lê os bytes 34 e 35 da estrutura de sincronização, converte para float e grava em `remote + 16`.
- Há gravações equivalentes do par em `0x5035ec` e `0x503bec` para outros estados de sincronização.

O mod lê esses dois floats após o processamento original, sem modificá-los, e associa o valor ao ponteiro CPed. O cache deixa de fornecer uma entrada após dois segundos sem atualização e remove a associação quando o wrapper desaparece. A barra usa 100 pontos como referência visual e limita o preenchimento à altura disponível. Acima de 100, a barra fica cheia; a lista ainda mostra o número recebido. Vida desconhecida é cinza, não é substituída pelo valor sintético 1000.

## Interface e toque

A biblioteca exporta **Dear ImGui 1.92.8**. `GetVersion`, em `0x5c5e50`, retorna essa versão. Foram conferidos os 16 símbolos usados pelo menu, incluindo `Begin`, `End`, `Button`, estilos e `EndFrame`.

O menu é submetido antes do `EndFrame` original, em `0x5d1140`, apenas dentro de um quadro ativo e uma vez por quadro/contexto. As pilhas de cores, estilos e janelas são restauradas antes de devolver o controle ao cliente. O tema usa fundo escuro, bordas retas e letras em bege, inspirado nos menus de San Andreas; não inclui fontes ou texturas extraídas do jogo.

O cliente já envia posição e botões para o ImGui: chamadas a `AddMousePosEvent` e `AddMouseButtonEvent` aparecem em `0x469784`, `0x469794`, `0x4697a4` e `0x4697b4`. O novo menu reutiliza esse fluxo. O comportamento de toque, inclusive interação com controles do jogo ou outras janelas, ainda precisa ser validado no aparelho.

Referência dos enums e prefixos do ImGui usados: https://github.com/ocornut/imgui/blob/v1.92.8/imgui.h

## Lista de jogadores próximos

A lista é uma inspeção dos personagens carregados no cliente, identificados pelo GTA como jogadores e associados a uma entrada sincronizada. Mostra ID, distância, vida e colete; ordena por distância. Selecionar um item apenas expande os dados. Não é uma relação de todos os jogadores conectados e pode omitir personagens fora do streaming ou ainda sem associação sincronizada.

## Limite da conclusão sobre dano

Alterar a vida local de um CPed e observar sua barra não demonstra que o estado de outro cliente ou do servidor mudou. A própria atribuição sintética de 1000 à vida local dos remotos demonstra por que não se pode interpretar esse campo como a autoridade da partida.

Também é necessário distinguir o envio de uma notificação de impacto/dano, a execução dos callbacks do servidor, as validações dos scripts e a aplicação efetiva da perda de vida. O resultado depende da implementação do servidor e dos clientes. Esta análise da biblioteca de um único cliente não confirma que o servidor aceitaria uma mensagem falsa como tiros válidos de sniper nem que isso retiraria a vida de outro jogador.

Esta versão não implementa envio de dano, tiros simulados, repetição de eventos de rede ou uma ação de ataque na lista. Para um teste reproduzível de dano no servidor local, ainda seria necessário analisar a implementação do servidor e registrar os eventos aceitos e a alteração de vida do alvo. A lista e os valores sincronizados entregues aqui servem para observar esse resultado durante um teste autorizado.
