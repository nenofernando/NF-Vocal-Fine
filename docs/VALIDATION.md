# Validação da prévia 0.2.2

Verificação local em 28/10/2026, macOS Intel (x86_64), JUCE 9.

**61 verificações passaram:** 47 do motor/host VST3 e 14 de interação com o editor.

## Interface

- Abertura padrão em 848 × 544 confirmada tanto no editor direto quanto no host VST3.
- Nome e descrição centralizados no cabeçalho.
- Alça azul visível, redimensionamento proporcional entre 636 × 408 e 1590 × 1020.
- Arraste para aumentar/diminuir e limites mínimo/máximo exercitados por eventos de mouse.
- Duplo clique no logo restaura 848 × 544. Clique simples e duplo clique fora do logo preservam o tamanho escolhido.
- Transformação do conteúdo preserva a escala de tela do host.
- Layout e enquadramento conferidos nos tamanhos mínimo, padrão e máximo.
- Duas curvas e régua de -50 a +50 cents: original azul, saída corrigida verde.
- Visual Halo duplo aprovado: luzes neon em uma única régua, sem traços ou triângulos móveis. O zero permanece fixo.
- Suavização e desaparecimento gradual aplicados somente à animação visual; as medições e o áudio permanecem intactos.
- A imagem da documentação usa um sinal sintético; nenhuma voz do usuário é publicada.

## Áudio e medições

- Afinação e correção verificadas em 44,1, 48 e 96 kHz.
- Latência de 120 ms confirmada por impulso, no áudio processado e no original.
- Estéreo, silêncio após reset, vibrato, escala e habilitação de notas.
- Duas medições reais de pitch; a origem é atrasada em 120 ms para comparar o mesmo instante da saída.
- Mix em zero produz leituras idênticas nas duas medições, inclusive ao mudar de nota.
- Habilitar a monitoração mantém o áudio numericamente idêntico ao processamento sem monitoração.
- A análise adicional só funciona com o editor aberto e é desligada ao fechá-lo.
- Reset limpa as leituras e o histórico é reconstruído ao abrir o editor.

## Host e distribuição

- Bundle VST3 carregado e instanciado em host de teste JUCE.
- 19 controles mais bypass e recuperação do estado salvo.
- Editor abre em 848 × 544 e anuncia suporte a redimensionamento ao host.
- Assinatura local ad-hoc verificada depois da geração do manifesto.
- Identificadores VST3 e de parâmetros preservados. Não instale duas prévias na mesma pasta de busca.

Os testes não certificam compatibilidade com todas as DAWs nem naturalidade em todas as vozes. A integração nativa na Mixa Studio aguarda aprovação do usuário.
