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
