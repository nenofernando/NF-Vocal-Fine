# Validação da prévia 0.3.0

Verificação local em 28/10/2026, macOS Intel (x86_64), JUCE 9.

86 verificações passaram: 49 de áudio/host VST3, 14 de interface e 23 do modo manual. O estado manual também foi inserido e recuperado através do estado de um VST3 realmente carregado pelo host de teste.

## Modo manual

23 verificações específicas passaram, cobrindo:

- Criar intervalo, substituir somente a parte sobreposta, remover e desfazer.
- Salvar/reabrir intervalos, deslocamentos e mapa de afinação.
- Áudio de C4 deslocado para B3 e C#4 apenas no intervalo selecionado, em 44,1 e 48 kHz.
- C#4 permitido manualmente mesmo com Dó maior na correção automática.
- Mesma nota antes e depois do intervalo preservada.
- Captura de afinação ligada ao tempo da sessão fornecido por transporte de teste.
- Abrir modo manual pelo cabeçalho, arrastar intervalo e clicar +1 semitom.
- Voltar à posição salva reaplica a edição; sair dela restaura a correção automática.
- Com transporte parado, os ajustes manuais não se aplicam à entrada ao vivo.
- Estado antigo sem dados manuais não cria edições.

Os testes usam sinais sintéticos e transporte JUCE de teste. Não substituem a avaliação dentro da DAW do usuário com voz real. A indicação verde no mapa manual é uma previsão do deslocamento, não análise de áudio renderizado.

## Interface e áudio existentes

A verificação de interface cobre 14 casos de abertura compacta, redimensionamento, limites, duplo clique no logo, escala do host e monitores de pitch. A suíte de áudio/host cobre 49 verificações de afinação, latência, estéreo, vibrato, curvas, estado e carregamento VST3.

Abertura padrão: 848 × 544. Limites: 636 × 408 a 1590 × 1020.
Latência do processamento: 120 ms. Identificadores VST3 e dos parâmetros preservados.

## Limites práticos

As edições dependem da posição de reprodução do host e são ligadas ao tempo da sessão. Mover clipes requer revisar os intervalos. A captura guarda metadados, sem copiar áudio. Limites: 36 mil pontos, 128 intervalos e 64 alterações no histórico de desfazer. O ajuste manual transpõe a voz original no intervalo; não quantiza rigidamente o vibrato nem segue a identidade dos clipes. Avalie especialmente notas curtas e as bordas da seleção.

Sem instalação global, sem publicação de gravações do usuário e sem integração nativa na Mixa Studio nesta etapa.
