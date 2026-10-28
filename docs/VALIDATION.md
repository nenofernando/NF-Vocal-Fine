# Validação da prévia 0.5.0

Teste local em 28/10/2026, macOS Intel, JUCE 9.

140 verificações passaram: 51 de áudio/host VST3, 14 de interface, 23 de edição por trecho, 33 de curvas/gestos/duplo clique e 19 de AUTOMATIC. Os sinais e imagens são sintéticos; gravações do usuário não são publicadas.

## Exclusão por duplo clique

Na ferramenta Mão / Pontos, o duplo clique exclui somente a âncora atingida, reconecta as vizinhas, permite desfazer e remove a linha ao apagar sua última âncora. O mouse-up do segundo clique não recria o ponto. Endpoint, ponto intermediário e última âncora foram exercitados por eventos de mouse.

## AUTOMATIC

A suíte específica verifica:

- Sugestões de Dó maior e Ré menor a partir de mapas vocais de teste.
- Recusa de inferência com uma nota repetida ou mapa cromático sem suporte diatônico suficiente.
- Sinalização de escalas relativas ambíguas como incertas.
- Geração de curvas dentro da seleção e aplicação atômica, com um único Desfazer.
- Preservação dos ajustes manuais fora da seleção.
- Persistência de curvas e da sugestão de escala.
- Áudio real de C# corrigido para C apenas no intervalo escolhido; antes/depois preservados.
- Seleção explícita, Selecionar tudo, botão AUTOMATIC, sugestão visível e opção de escolher Ré menor manualmente.
- Recuperação da sugestão por meio do estado de um VST3 realmente carregado no host de teste.

A inferência é heurística, baseada nas classes/durações das notas do mapa capturado e suporte a escalas maior/menor natural. Não há análise harmônica do acompanhamento. Uma voz isolada não garante o tom original da música; confira a sugestão, especialmente quando houver '?', ou escolha a escala manualmente. Casos sem base suficiente não forçam uma escala.

## Regressão

Mantidos os testes de pontos, linhas, lápis, gesto completo de desfazer, edições por posição, captura, persistência, intervalos, áudio em múltiplas taxas, latência, estéreo, vibrato, redimensionamento e reset do tamanho pelo logo.

Abertura 848 × 544; limites 636 × 408 a 1590 × 1020; latência de áudio 120 ms. O host informa a posição na sessão. Mover clipes exige revisar os intervalos. Verde no editor é alvo previsto; o monitor compara as medições reais. Sem instalação global e sem integração nativa na Mixa Studio nesta etapa.
