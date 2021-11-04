# EP1 - MAC0352 - Redes de Computadores e Sistemas Distribuídos (2021)

## Como compilar

Para compilar o EP, execute o comando "make" no diretório principal do projeto (onde se encontra este arquivo de LEIAME) em um sistema GNU/Linux. Isso irá gerar o executável "broker", que é o broker/servidor que foi implementado.

## Como rodar

Para rodar o servidor, há duas maneiras:

1. Execute o seguinte comando no mesmo diretório onde o executável "broker" se encontra:
    $ ./broker [PORTA]
onde [PORTA] é o número da porta a qual deseja-se   que o servidor escute requisições

2. Caso deseje escutar na porta padrão do protocolo MQTT (porta 1883), pode-se também executar o comando:
    $ make run
sendo que esse comando é só uma chamada para o comando No.1 com a porta especificada para 1883


## Estrutura do projeto

O projeto tem dois diretórios: uma pasta "include" com todos os arquivos de extensão .h e uma pasta "src" com todos os arquivos de extensão. Considerando que muitos arquivo de extensão .h são somente para declarações e importação de biblioteca (apesar de que nem todos são assim), abaixo está uma descrição desses arquivos:

--> broker_client(.c|.h)
    Define o funcionamento das "client threads", threads responsáveis pela conexão entre os processos clientes e a lógica interna do broker existente na "manager thread".

--> broker_manager(.c|.h)
    Define o funcionamento da "manager thread", unidade central do broker responsável pelo controle das conexões e redirecionamento dos pacotes de PUBLISH.

--> message_queue(.c|.h)
    Implementação de uma fila utilizando uma lista ligada. Serve especificamente para ser a fila de mensagens associadas a cada thread e que permite comunicação entre uma "client thread" e a "manager thread".

--> misc(.c|.h)
    Um conjunto de funções com vários propósitos. Várias funções de manipulação de bits, muito utilizadas durante o "parsing" das sequências de bytes vindas das conexões com cliente para uma estrutura mais compreensível.

--> mqtt_conf.h
    Arquivo de definição de MACROS relacionados a limites impostos a estruturas internas do broker

--> mqtt_(connack | connect | disconnect | ping | publish | suback | subscribe)(.c|.h)
    Conjunto de arquivos que
        1. Ou realizam "parsing" em um conjunto de bytes para transcrever as informações vindas do cliente em algo mais compreensível internamente (uma struct);
        2. Ou, ao receber essa struct, formar uma resposta para o cliente.
    Uma exceção a isso é o mqtt_ping(.c|.h) que, pela sua simplicidade, trata tanto do pacote PINGREQ quanto PINGRESP

    Além disso, definem a estrutura das mensagens trocadas entre "client threads" e a "manager thread".

--> mqtt(.c|.h)

    Em sua maioria, definem coisas relacionadas ao protocolo MQTT em si.

--> packet_queue(.c|.h)

    Implementação de uma fila utilizando uma lista ligada com o propósito de armazenar um número de pacotes (conjunto de bytes). Utilizado pela operação de leitura do socket da conexão com o cliente, visto que, por ser uma leitura "non-blocking", há a chance de leitura de duas mensagens diferentes de uma vez só.

## Notas adicionais

Para remover o executável "broker", pode-se executar o comano:
    $ make clean
que irá o removê-lo

