import styled from "styled-components";

export const PageHeader = styled.h1`
    font-weight: 700;
    font-size: 36px;
    margin: 40px 20px;
    text-transform: uppercase;
    @media only screen and (min-width: 900px) {
        margin: 40px auto;
        max-width: 1200px;
        position: relative;
    }
    &:after {
        background-color: rgb(253,209,0);
        content: ' ';
        display: block;
        height:2px;
        max-width: 40px;
        position: relative;
        top: 5px;
        width: 13%;
    }
`;

export const Loading = styled.div`
    align-items: center;
    display: flex;
    font-size: 36px;
    font-weight: 700;
    height: 100vh;
    justify-content: center;
    width: 100%;
`

export const Section = styled.section`
    border: 1px solid #302D2D;
    border-radius: 8px;
    margin: 0 auto;
    padding: 80px 20px 30px;
    position: relative;
    width: calc(50% - 15px);
    &.full {
        width: calc(100% - 15px);
    }
    h2 {
        background-color: #302D2D;
        border-radius: 8px 8px 0 0;
        color: #fff;
        font-weight: 700;
        left: 0;
        padding: 20px;
        position: absolute;
        top: 0;
        width: 100%;
    }
    .save {
        display: flex;
        margin: 20px auto 0;
        max-width: 200px;
    }
    .cancel {
        background-color: #302D2D;
        color: #fff;
        display: flex;
        margin: 20px auto 0;
        max-width: 200px;
    }
`;

export const ButtonSolid = styled.button`
    align-items: center;
    background-color: rgb(253,209,0);
    border: 1px solid rgb(253,209,0);
    color: #000;
    display: inline-flex;
    font-size: 16px;
    height: 40px;
    justify-content: center;
    padding: 0 20px;
    text-align: center;
    text-transform: uppercase;
    width: 100%;
    &.inline {
        width: auto;
    }
    &.mb {
        margin-bottom: 10px;
    }
    &.mt {
        margin-top: 10px;
    }
    &[disabled] {
        opacity: 0.5;
    }
`;

export const BorderedList = styled.ul`
    &.nopadd {
        li {
            padding: 20px 0;
        }
    }
    &.col3 {
        display: flex;
        flex-flow: row;
        flex-wrap: wrap;
        li {
            max-width: 33%;
            width: 100%;
            &.fullCenter {
                text-align: center;
                max-width: 100%;
                button {
                    margin: 10px auto 0;
                    max-width: 300px;
                }
            }
        }
    }
    &.col2 {
        display: flex;
        flex-flow: row;
        flex-wrap: wrap;
        li {
            max-width: 50%;
            padding-right: 20px;
            width: 100%;
            &.fullCenter {
                text-align: center;
                max-width: 100%;
                button {
                    margin: 10px auto 0;
                    max-width: 300px;
                }
            }
        }
    }
    &.mb {
        margin-bottom: 50px;
    }
    &.pr li {
        padding-right: 20px;
    }
    &.center li {
        display: flex;
        flex-flow: column;
        justify-content: end;
        text-align: left;
    }
    li {
        border-bottom: 1px solid rgba(255, 255, 255, .5);
        padding: 20px;
        &.lastItem {
            border-bottom: none;
        }
        p {
            margin-bottom: 20px;
        }
        span {
            display: inline-flex;
            margin-right: 15px;
        }
        select {
            margin-top: 8px;
        }
        label {
            position: relative;
            input[type='checkbox'] {
                left: -9999px;
                position: absolute;
                &:checked + .checkboxIcon:before {
                    background-color: rgb(253,209,0);
                    content: " ";
                    cursor: pointer;
                    display: block;
                    height: 12px;
                    left: 3px;
                    position: absolute;
                    top: 3px;
                    width: 12px;
                    z-index: 2;
                }
            }
            input[type='radio'] {
                left: -9999px;
                position: absolute;
                &:checked + .radioIcon:before {
                    background-color: rgb(253,209,0);
                    border-radius: 20px;
                    content: " ";
                    cursor: pointer;
                    display: block;
                    height: 12px;
                    left: 3px;
                    position: absolute;
                    top: 3px;
                    width: 12px;
                    z-index: 2;
                }
            }
            input, select {
                &[disabled] {
                    opacity: 0.5;
                }
            }
            &.checkbox {
                padding-left: 30px;
                .checkboxIcon:after {
                    background-color: #0f0f0f;
                    border: 1px solid rgb(253,209,0);
                    content: " ";
                    cursor: pointer;
                    display: block;
                    height: 16px;
                    left: 0;
                    position: absolute;
                    top: 0;
                    width: 16px;
                }
                .radioIcon:after {
                    background-color: #0f0f0f;
                    border: 1px solid rgb(253,209,0);
                    border-radius: 20px;
                    content: " ";
                    cursor: pointer;
                    display: block;
                    height: 16px;
                    left: 0;
                    position: absolute;
                    top: 0;
                    width: 16px;
                }
            }
        }
        &.col2 {
            display: flex;
            gap: 10px;
            justify-content: baseline;
            div {
                display: flex;
    flex-flow: column;
                justify-content: end;
                width: 48%;
            }
        }
        &.accent {
            background-color: #302D2D;
            &.firstPadd {
                padding-left: 20px;
            }
        }
    }
    .listItemChild {
        border-left: 5px solid rgba(255, 255, 255, .5);
        margin-top: 20px;
        padding-left: 20px;
        li:first-child {
            padding-top: 0;
        }
    }
`;
