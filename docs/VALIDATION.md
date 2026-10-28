# Validação da prévia 0.4.0

Teste local em 28/10/2026, macOS Intel, JUCE 9.

115 verificações passaram: 50 de áudio/host VST3, 14 de interface, 23 de edição por trecho e 28 de pontos/curvas/lápis. Os sinais e imagens de teste são sintéticos; nenhuma gravação do usuário é publicada no repositório.

## Pontos, linhas e lápis

28 verificações específicas cobrem:

- Criação de pontos ligados e interpolação linear da altura entre eles.
- Um ponto isolado não produz alteração; a linha precisa de pelo menos dois.
- Mover um ponto altera o alvo interpolado; seleção sem mudança não cria etapa inútil de desfazer.
- Arrastar a linha move os dois pontos que a delimitam.
- Curva livre, exclusão, recorte de uma seleção, preservação das partes externas e desfazer do gesto inteiro.
- Rejeição de pontos inválidos/duplicados e persistência dos nós editáveis.
- Áudio real C4 -> C#4 somente dentro do intervalo desenhado, fora de Dó maior, em 44,1/48 kHz.
- Áudio acompanha uma linha ascendente de altura entre pontos.
- Gestos reais de mouse da mão e do lápis na área Manual ampliada.
- Estado do processador recupera as curvas após salvar/reabrir.

## Compatibilidade das funções anteriores

São mantidos os testes de edição por trecho, posição da sessão, escala automática, curvas medidas, latência, estéreo, estado e redimensionamento. A verificação do host VST3 inclui o carregamento do arquivo gerado e a recuperação dos pontos de uma curva através do estado real do VST3.

Abertura 848 × 544; limites 636 × 408 a 1590 × 1020; duplo clique no logo restaura o tamanho. O modo Manual amplia a área de desenho dentro da janela, sem alterar o modo Monitor. Latência de áudio: 120 ms.

## Limites de interpretação

Verde no editor é altura alvo prevista, não áudio já renderizado. O monitor compara as medições reais. Curvas são ligadas à posição na sessão, exigem tempo/transporte válidos do host e não seguem automaticamente clipes movidos. A diferença de altura aplicada é limitada a ±12 semitons da voz detectada; sem pitch detectável, a curva não inventa uma nota.

Há até 64 linhas e 4096 pontos de curva, além de 128 intervalos e 36 mil pontos do mapa capturado. Desfazer guarda 64 alterações. Notas muito curtas, curvas rápidas e limites de sílabas exigem avaliação auditiva. A validação automatizada não certifica todas as DAWs nem naturalidade para todos os cantores.

Sem instalação global e sem integração nativa na Mixa Studio nesta etapa.
