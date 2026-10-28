# NF Vocal Fine

Plugin de afinação vocal da **NF Audio Tools**, em VST3, com interface escura em azul e prata e o logo original da marca.

![Interface do NF Vocal Fine](Assets/Interface.png)

Imagem obtida com um sinal sintético de teste, mostrando as duas medições reais de afinação.

A versão **0.2.2** aplica o visual aprovado Halo duplo à régua: duas luzes neon na mesma faixa, azul para o original e verde para a voz corrigida, com deslocamento suave e desaparecimento gradual sem sinal. Mantém a interface compacta, título centralizado e os dois monitores de afinação. O processamento de afinação mantém os mesmos controles: correção por tom e escala, seleção de notas, intensidade, tempo de transição, preservação de vibrato, compensação de formantes e mistura com a voz original.

## Primeiro teste

Use uma voz solo gravada, sem acompanhamento ou efeitos no mesmo arquivo. Escolha o tom e a escala da música. A configuração inicial é Correção 80%, Transição 110 ms, Naturalidade 70%, Formantes 0 st e Mix 100%.

O monitor compara duas medições reais de afinação: **azul = voz original** e **verde = saída corrigida**, incluindo a posição do controle Mix. O original é atrasado internamente pela latência do processamento para as duas curvas representarem o mesmo instante do áudio. O valor à direita mostra a diferença medida entre as duas vozes quando há nota detectada. A régua horizontal, de -50 a +50 cents, usa luzes neon nas mesmas cores e marca o zero no centro; o histórico acima mostra até ±100 cents em relação à nota mais próxima do original.

## Tamanho da interface

A abertura padrão é **848 × 544**, 20% menor em cada dimensão que a prévia 0.2.0. Arraste a alça azul no canto inferior direito para aumentar ou diminuir proporcionalmente, entre 636 × 408 e 1590 × 1020. **Dois cliques no logo NF** restauram o tamanho padrão. O redimensionamento do conteúdo preserva a escala de tela definida pelo host.

Os analisadores adicionais de afinação funcionam enquanto o editor está aberto e não modificam o sinal de áudio.

A prioridade desta prévia é avaliar a qualidade em voz gravada. A latência é de **120 ms**, informada ao host para compensação, e pode incomodar durante monitoração ao vivo. A naturalidade ainda depende de teste auditivo com voz real. Não é um afinador polifônico nem um editor manual de notas.

## Compilar

Requer CMake 3.22+, compilador C++17 e um checkout externo do JUCE 9. Signalsmith Stretch e Signalsmith Linear estão incluídos em `ThirdParty/` com suas licenças.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=/caminho/para/JUCE
cmake --build build --target NFVocalFine_VST3 -j 6
```

No macOS, o bundle fica em `build/NFVocalFine_artefacts/Release/VST3/NF Vocal Fine.vst3`. Depois da geração de `moduleinfo.json`, aplique a assinatura local:

```sh
codesign --force --deep --sign - "build/NFVocalFine_artefacts/Release/VST3/NF Vocal Fine.vst3"
codesign --verify --deep --strict "build/NFVocalFine_artefacts/Release/VST3/NF Vocal Fine.vst3"
```

Copie o bundle para `~/Library/Audio/Plug-Ins/VST3/` e faça a DAW buscar plugins novamente. Esta prévia é assinada localmente e não é notarizada pela Apple. A compilação local entregue é para Mac Intel (x86_64); uma versão Apple Silicon ou Windows precisa de compilação e validação próprias.

Para evitar duas cópias da mesma prévia no host, retire o VST3 antigo NF Vocal Natural da pasta de busca ao instalar o NF Vocal Fine. Guarde-o como backup. A atualização mantém os IDs de parâmetros e o formato de estado; a compatibilidade completa de sessões antigas em cada DAW ainda precisa ser verificada.

## Testes

```sh
cmake -S . -B build -DNF_BUILD_TESTS=ON -DJUCE_DIR=/caminho/para/JUCE
cmake --build build --target VocalFineTests VocalFineEditorChecks -j 6
build/VocalFineTests_artefacts/Release/VocalFineTests "$(pwd)/build/NFVocalFine_artefacts/Release/VST3/NF Vocal Fine.vst3"
build/VocalFineEditorChecks_artefacts/Release/VocalFineEditorChecks "$(pwd)/work/ui-checks"
```

As verificações cobrem detecção de notas, áudio corrigido em 44,1/48/96 kHz, latência por impulso, estéreo, reset, vibrato, escalas, carregamento do VST3, parâmetros, persistência, abertura compacta no host, medições das curvas e ausência de mudanças no áudio ao ativar a monitoração. Os testes do editor exercitam a alça, os limites de tamanho, o duplo clique no logo, a escala de tela e o enquadramento dos controles. O host de teste é baseado em JUCE; isso não substitui o teste dentro da DAW do usuário.

## Organização

- `Source/Engine.h`: motor de áudio reutilizável.
- `Source/Plugin.cpp`: parâmetros, estado, wrapper e interface.
- `Source/FineLook.h`: desenho dos controles.
- `Source/Tests.cpp`: testes do motor e do VST3.
- `Source/EditorChecks.cpp`: testes de interação e imagens de verificação do editor.
- `Assets/Logo.png`: logo original fornecido pelo usuário, incorporado ao binário.
- `ThirdParty/`: bibliotecas e termos de terceiros.

A integração nativa na Mixa Studio fica para depois da aprovação do VST3. Este projeto não altera a Mixa, não instala plugins automaticamente e não tem workflows de publicação.

## Dependências e identidade visual

O logo é da NF Audio Tools e não recebe a licença das bibliotecas de terceiros. JUCE é fornecido sob AGPLv3 ou licença comercial. Para distribuição fechada, pode ser necessária uma licença comercial aplicável do JUCE. Consulte `ThirdParty/JUCE-LICENSE.md` e os termos do JUCE antes de distribuir. Signalsmith Stretch e Linear mantêm suas licenças MIT incluídas nos respectivos diretórios.
